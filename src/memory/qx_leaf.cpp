// =============================================================================
// QXEngine Core – src/memory/qx_leaf.cpp
// Owner: Masa Bayu
// Created: 2026-05-24
// Description: Implementation of the QXLeaf allocation unit – the smallest
//              named memory object managed by the QXEngine. Enforces Law 1
//              (Pattern) via label validation, Law 3 (Pairs) via state
//              tracking, and the class-A eviction prohibition at all tiers.
//
// Repository: https://github.com/qxengine/qxengine-core
// =============================================================================

#include "qxengine/memory/qx_leaf.h"
#include "qxengine/core/qx_constants.h"
#include "qxengine/core/qx_error.h"
#include "qxengine/core/qx_types.h"

#include <cstring>      // std::strlen, std::strncpy, std::memset
#include <cstdio>       // std::snprintf
#include <ctime>        // std::chrono via platform
#include <cassert>      // assert
#include <mutex>        // std::mutex, std::lock_guard
#include <atomic>       // std::atomic<uint64_t>
#include <chrono>       // std::chrono::system_clock


// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

namespace {

// ── Timestamp ─────────────────────────────────────────────────────────────────

/**
 * @brief Return current wall-clock time in milliseconds since Unix epoch.
 */
static QXTimestamp now_ms() noexcept {
    using namespace std::chrono;
    return static_cast<QXTimestamp>(
        duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()
        ).count()
    );
}

// ── Label validation ──────────────────────────────────────────────────────────

/**
 * @brief Check whether a label string consists only of whitespace.
 *
 * @param label  Null-terminated string.
 * @return true if blank or whitespace-only.
 */
static bool is_blank(const char *label) noexcept {
    if (!label) { return true; }
    for (const char *p = label; *p != '\0'; ++p) {
        if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
            return false;
        }
    }
    return true;
}

/**
 * @brief Validate a leaf label against Law 1 (Pattern) rules.
 *
 * Rules:
 *   1. Must not be NULL.
 *   2. Must not be blank or whitespace-only → QX_ERR_LABEL_BLANK.
 *   3. Length (trimmed) must be ≥ QX_LABEL_MIN_LENGTH (3) →
 *      QX_ERR_LABEL_TOO_SHORT.
 *   4. Length (trimmed) must be ≤ QX_LABEL_MAX_LENGTH (128) →
 *      QX_ERR_LABEL_TOO_LONG.
 *
 * @param label  Null-terminated UTF-8 label string.
 * @return QX_OK if valid; appropriate QX_ERR_LABEL_* on failure.
 */
static QXResult validate_label(const char *label) noexcept {
    if (!label || is_blank(label)) {
        return QX_ERR_LABEL_BLANK;
    }
    const std::size_t len = std::strlen(label);
    if (len < static_cast<std::size_t>(QX_LABEL_MIN_LENGTH)) {
        return QX_ERR_LABEL_TOO_SHORT;
    }
    if (len > static_cast<std::size_t>(QX_LABEL_MAX_LENGTH)) {
        return QX_ERR_LABEL_TOO_LONG;
    }
    return QX_OK;
}

// ── UUID generation ───────────────────────────────────────────────────────────

/**
 * @brief Monotonically increasing leaf sequence counter.
 *
 * Used to produce deterministic UUIDs in the format:
 *   "leaf-XXXXXXXXXXXXXXXX" (leaf- prefix + 16-char zero-padded hex).
 * Full RFC-4122 UUID generation is intentionally avoided to eliminate
 * OS-level randomness dependencies in the core library.
 */
static std::atomic<uint64_t> g_leaf_sequence{ 1u };

/**
 * @brief Write a deterministic leaf UUID into @p buf (37-byte buffer).
 *
 * Format: "leaf-0000000000000001" (20 chars + null = 21 bytes used,
 * padded to 37 bytes for ABI compatibility with QX_TEL_LEAF_ID_MAX).
 *
 * @param buf  Caller-provided buffer of at least 37 bytes.
 */
static void generate_leaf_id(char *buf) noexcept {
    const uint64_t seq = g_leaf_sequence.fetch_add(1u,
                             std::memory_order_relaxed);
    std::snprintf(buf, 37u, "leaf-%016llx",
                  static_cast<unsigned long long>(seq));
}

// ── Eviction eligibility matrix ───────────────────────────────────────────────

/**
 * @brief Return QX_TRUE if a leaf class is evictable at a given pressure tier.
 *
 * Authoritative matrix (from qx_pressure.h):
 *
 *   Class \ Tier   1    2    3    4
 *   A Protected    ✗    ✗    ✗    ✗
 *   B Essential    ✗    ✗    ✗    ✓
 *   C Standard     ✗    ✗    ✓    ✓
 *   D Transient    ✗    ✓    ✓    ✓
 *
 * @param leaf_class  Leaf class (QX_LEAF_CLASS_A through QX_LEAF_CLASS_D).
 * @param tier        Pressure tier (1–4).
 * @return QX_TRUE if evictable; QX_FALSE otherwise.
 */
static QXBool evictable_at_tier(QXLeafClassId leaf_class,
                                 QXTierId      tier) noexcept {
    switch (leaf_class) {
        case QX_LEAF_CLASS_A: return QX_FALSE;
        case QX_LEAF_CLASS_B: return (tier >= 4) ? QX_TRUE : QX_FALSE;
        case QX_LEAF_CLASS_C: return (tier >= 3) ? QX_TRUE : QX_FALSE;
        case QX_LEAF_CLASS_D: return (tier >= 2) ? QX_TRUE : QX_FALSE;
        default:              return QX_FALSE;
    }
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXLeafImpl (internal representation)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Internal heap-allocated state for a QXLeaf.
 *
 * QXLeafHandle (QXLeaf_s*) points to this struct. All public API functions
 * cast the opaque handle to QXLeafImpl* internally.
 */
struct QXLeaf_s {
    // ── Identity (immutable after creation) ───────────────────────────────────
    char            leaf_id[37];
    char            label[QX_LABEL_MAX_LENGTH + 1];
    char            segment_id[16];
    QXLeafClassId   leaf_class;
    QXSize          size_bytes;
    uint32_t        slot_index;
    QXTimestamp     allocated_at_ms;

    // ── Mutable state ─────────────────────────────────────────────────────────
    QXLeafState     state;
    QXTimestamp     last_accessed_at_ms;
    uint64_t        touch_count;

    // ── Transition history (max 16 entries) ───────────────────────────────────
    QXLeafTransition    transitions[16];
    uint32_t            transition_count;

    // ── Thread safety ─────────────────────────────────────────────────────────
    mutable std::mutex  mtx;
};

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Internal factory (non-exported)
// ─────────────────────────────────────────────────────────────────────────────

namespace qx::internal {
    QXResult leaf_create(const char*, const char*, QXLeafClassId, QXSize, uint32_t, QXLeafHandle*) noexcept;
    void     leaf_destroy(QXLeafHandle) noexcept;
    QXResult leaf_transition(QXLeafHandle, QXLeafState, const char*) noexcept;
} // namespace qx::internal

namespace qx::internal {

/**
 * @brief Create and initialise a new QXLeaf.
 *
 * Validates the label, assigns a UUID, and sets initial state to ACTIVE.
 * The caller takes ownership of the returned handle and is responsible
 * for calling qx_leaf_destroy() exactly once.
 *
 * @param label       Null-terminated label (validated against Law 1).
 * @param segment_id  Null-terminated segment ID (max 15 chars).
 * @param leaf_class  Leaf class A–D.
 * @param size_bytes  Allocation size in bytes (must be > 0).
 * @param slot_index  Slot index within the segment (0–59).
 * @param out_leaf    Receives the new leaf handle on success.
 * @return QX_OK on success; QX_ERR_LABEL_* or QX_ERR_INTERNAL on failure.
 */
QXResult leaf_create(
    const char   *label,
    const char   *segment_id,
    QXLeafClassId leaf_class,
    QXSize        size_bytes,
    uint32_t      slot_index,
    QXLeafHandle *out_leaf
) noexcept {
    if (!label || !segment_id || !out_leaf) {
        return QX_ERR_NULL_HANDLE;
    }
    if (size_bytes == 0u) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    const QXResult label_result = validate_label(label);
    if (label_result != QX_OK) { return label_result; }

    auto *leaf = new (std::nothrow) QXLeaf_s{};
    if (!leaf) { return QX_ERR_INTERNAL; }

    generate_leaf_id(leaf->leaf_id);
    std::strncpy(leaf->label,      label,      QX_LABEL_MAX_LENGTH);
    std::strncpy(leaf->segment_id, segment_id, 15u);
    leaf->label[QX_LABEL_MAX_LENGTH] = '\0';
    leaf->segment_id[15]             = '\0';

    leaf->leaf_class       = leaf_class;
    leaf->size_bytes       = size_bytes;
    leaf->slot_index       = slot_index;
    leaf->allocated_at_ms  = now_ms();
    leaf->state            = QX_LEAF_STATE_ACTIVE;
    leaf->last_accessed_at_ms = leaf->allocated_at_ms;
    leaf->touch_count      = 0u;
    leaf->transition_count = 0u;

    *out_leaf = leaf;
    return QX_OK;
}

/**
 * @brief Destroy a QXLeaf and release all internal resources.
 *
 * @param leaf  Handle to destroy. No-op if NULL.
 */
void leaf_destroy(QXLeafHandle leaf) noexcept {
    delete leaf;
}

} // namespace qx::internal

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Public C ABI
// ─────────────────────────────────────────────────────────────────────────────

extern "C" {

// ── Info ──────────────────────────────────────────────────────────────────────

QX_API QXResult qx_leaf_info(
    QXLeafHandle  leaf,
    QXLeafInfo   *out_info
) {
    if (!leaf || !out_info) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(leaf->mtx);

    const QXTimestamp now = now_ms();

    std::strncpy(out_info->leaf_id,    leaf->leaf_id,    36u);
    std::strncpy(out_info->label,      leaf->label,      128u);
    std::strncpy(out_info->segment_id, leaf->segment_id, 15u);
    out_info->leaf_id[36]    = '\0';
    out_info->label[128]     = '\0';
    out_info->segment_id[15] = '\0';

    out_info->leaf_class        = leaf->leaf_class;
    out_info->state             = leaf->state;
    out_info->size_bytes        = leaf->size_bytes;
    out_info->slot_index        = leaf->slot_index;
    out_info->allocated_at_ms   = leaf->allocated_at_ms;
    out_info->last_accessed_at_ms  = leaf->last_accessed_at_ms;
    out_info->touch_count       = leaf->touch_count;
    out_info->transition_count  = leaf->transition_count;
    out_info->is_evictable      = evictable_at_tier(leaf->leaf_class,
                                                     QX_TIER_CRITICAL);

    // Derived time fields
    const double age_ms  = static_cast<double>(now - leaf->allocated_at_ms);
    const double idle_ms = static_cast<double>(now - leaf->last_accessed_at_ms);
    out_info->age_seconds  = static_cast<uint64_t>(age_ms  / 1000u);
    out_info->idle_seconds = static_cast<uint64_t>(idle_ms / 1000u);

    return QX_OK;
}

// ── State accessors ───────────────────────────────────────────────────────────

QX_API QXResult qx_leaf_state(
    QXLeafHandle  leaf,
    QXLeafState  *out_state
) {
    if (!leaf || !out_state) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(leaf->mtx);
    *out_state = leaf->state;
    return QX_OK;
}

QX_API QXResult qx_leaf_id(
    QXLeafHandle   leaf,
    char*          buffer
) {
    if (!leaf || !buffer) { return QX_ERR_NULL_HANDLE; }

    std::lock_guard<std::mutex> lock(leaf->mtx);
    std::strncpy(buffer, leaf->leaf_id, 36u);
    buffer[36] = '\0';
    
    return QX_OK;
}

QX_API QXResult qx_leaf_label(
    QXLeafHandle   leaf,
    char*          buffer
) {
    if (!leaf || !buffer)              { return QX_ERR_NULL_HANDLE; }

    std::lock_guard<std::mutex> lock(leaf->mtx);
    std::strncpy(buffer, leaf->label, QX_LABEL_MAX_LENGTH);
    buffer[QX_LABEL_MAX_LENGTH] = '\0';
    
    return QX_OK;
}

QX_API QXResult qx_leaf_size_bytes(
    QXLeafHandle  leaf,
    QXSize       *out_bytes
) {
    if (!leaf || !out_bytes) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(leaf->mtx);
    *out_bytes = leaf->size_bytes;
    return QX_OK;
}

QX_API QXResult qx_leaf_class(
    QXLeafHandle    leaf,
    QXLeafClassId  *out_class
) {
    if (!leaf || !out_class) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(leaf->mtx);
    *out_class = leaf->leaf_class;
    return QX_OK;
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

QX_API QXResult qx_leaf_touch(QXLeafHandle leaf) {
    if (!leaf) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(leaf->mtx);
    if (leaf->state != QX_LEAF_STATE_ACTIVE) { return QX_ERR_NOT_READY; }
    leaf->last_accessed_at_ms = now_ms();
    leaf->touch_count     += 1u;
    return QX_OK;
}

// ── Transition history ────────────────────────────────────────────────────────

QX_API QXResult qx_leaf_transition(
    QXLeafHandle      leaf,
    uint32_t          index,
    QXLeafTransition *out_transition
) {
    if (!leaf || !out_transition) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(leaf->mtx);
    if (index >= leaf->transition_count) {
        return QX_ERR_INVALID_ARGUMENT;
    }
    *out_transition = leaf->transitions[index];
    return QX_OK;
}

QX_API QXResult qx_leaf_transition_count(
    QXLeafHandle  leaf,
    uint32_t     *out_count
) {
    if (!leaf || !out_count) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(leaf->mtx);
    *out_count = leaf->transition_count;
    return QX_OK;
}

// ── Convenience predicates ────────────────────────────────────────────────────

QX_API QXBool qx_leaf_is_active(QXLeafHandle leaf) {
    if (!leaf) { return QX_FALSE; }
    std::lock_guard<std::mutex> lock(leaf->mtx);
    return (leaf->state == QX_LEAF_STATE_ACTIVE) ? QX_TRUE : QX_FALSE;
}

QX_API QXBool qx_leaf_evictable_at_tier(
    QXLeafHandle leaf,
    QXTierId     tier
) {
    if (!leaf) { return QX_FALSE; }
    std::lock_guard<std::mutex> lock(leaf->mtx);
    if (leaf->state != QX_LEAF_STATE_ACTIVE) { return QX_FALSE; }
    return evictable_at_tier(leaf->leaf_class, tier);
}

} // extern "C"

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Internal state-transition helper (non-exported)
// ─────────────────────────────────────────────────────────────────────────────

namespace qx::internal {

/**
 * @brief Perform a validated state transition on a leaf.
 *
 * Records the transition in the leaf's history (capped at 16 entries).
 * Caller must NOT hold leaf->mtx before calling this function.
 *
 * Valid transitions:
 *   ACTIVE → EVICTED   (reason: pressure tier N eviction)
 *   ACTIVE → RELEASED  (reason: explicit deallocation)
 *   EVICTED → RELEASED (reason: orphan flush or deallocation after eviction)
 *
 * Invalid:
 *   RELEASED → any     → QX_ERR_DOUBLE_FREE
 *   EVICTED  → EVICTED → QX_ERR_INTERNAL
 *   Class-A  → EVICTED → QX_ERR_PROTECTED_EVICTION
 *
 * @param leaf    Non-NULL leaf handle.
 * @param to      Target state.
 * @param reason  Null-terminated reason string (max 63 chars).
 * @return QX_OK on success; appropriate error code on failure.
 */
QXResult leaf_transition(
    QXLeafHandle leaf,
    QXLeafState  to,
    const char  *reason
) noexcept {
    if (!leaf) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(leaf->mtx);

    const QXLeafState from = leaf->state;

    // Guard: released leaf cannot transition further
    if (from == QX_LEAF_STATE_RELEASED) {
        return QX_ERR_DOUBLE_FREE;
    }

    // Guard: class-A leaf cannot be evicted
    if (to == QX_LEAF_STATE_EVICTED && leaf->leaf_class == QX_LEAF_CLASS_A) {
        return QX_ERR_PROTECTED_EVICTION;
    }

    // Guard: already evicted cannot be evicted again
    if (from == QX_LEAF_STATE_EVICTED && to == QX_LEAF_STATE_EVICTED) {
        return QX_ERR_INTERNAL;
    }

    // Record transition (ring-cap at 16)
    if (leaf->transition_count < 16u) {
        QXLeafTransition &t = leaf->transitions[leaf->transition_count++];
        t.from         = from;
        t.to           = to;
        t.timestamp_ms = now_ms();
        if (reason) {
            std::strncpy(t.reason, reason, 63u);
            t.reason[63] = '\0';
        } else {
            t.reason[0] = '\0';
        }
    }

    leaf->state = to;
    return QX_OK;
}

} // namespace qx::internal
