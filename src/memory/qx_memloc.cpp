// =============================================================================
// QXEngine Core – src/memory/qx_memloc.cpp
// Owner: Masa Bayu
// Created: 2026-05-24
// Description: Implementation of QXMemloc – the central allocation engine
//              managing 60 QLM slots across nine segments. Enforces Laws
//              1–4 (Pattern, Limit, Pairs, Equilibrium) on every operation.
//
// Repository: https://github.com/qxengine/qxengine-core
// =============================================================================

#include "qxengine/memory/qx_memloc.h"
#include "qxengine/memory/qx_segment.h"
#include "qxengine/memory/qx_leaf.h"
#include "qxengine/core/qx_constants.h"
#include "qxengine/core/qx_error.h"
#include "qxengine/core/qx_types.h"

#include <cstring>      // std::strncpy, std::strcmp, std::memset
#include <mutex>        // std::mutex, std::lock_guard
#include <shared_mutex> // std::shared_mutex, std::shared_lock
#include <new>          // std::nothrow

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Forward declarations (internal)
// ─────────────────────────────────────────────────────────────────────────────

namespace qx::internal {
    QXResult leaf_create(const char*, const char*, QXLeafClassId,
                         QXSize, uint32_t, QXLeafHandle*) noexcept;
    void     leaf_destroy(QXLeafHandle) noexcept;
    QXResult leaf_transition(QXLeafHandle, QXLeafState,
                             const char*) noexcept;
    QXResult segment_create(const char*, uint32_t, QXSize,
                            QXSize, QXSegmentHandle*) noexcept;
    void     segment_destroy(QXSegmentHandle) noexcept;
    QXResult segment_allocate(QXSegmentHandle, QXLeafHandle,
                              QXSize, uint32_t*) noexcept;
    QXResult segment_deallocate(QXSegmentHandle, QXLeafHandle,
                                QXSize*) noexcept;
    QXResult segment_evict(QXSegmentHandle, QXLeafHandle,
                           QXSize*) noexcept;
    QXResult segment_eviction_candidates(QXSegmentHandle, QXTierId,
                                         QXLeafHandle*, uint32_t,
                                         uint32_t*) noexcept;
    int      segment_index(const char*) noexcept;
    uint32_t segment_slot_count(uint32_t) noexcept;
} // namespace qx::internal

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

namespace {

/**
 * @brief Find the segment handle and local slot for a given leaf handle.
 *
 * Scans all nine segments until the leaf is found.
 *
 * @param segments      Array of QX_SEGMENT_COUNT segment handles.
 * @param leaf          Leaf handle to locate.
 * @param out_segment   Receives the segment handle if found.
 * @return QX_OK if found; QX_ERR_LEAF_NOT_FOUND otherwise.
 */
[[maybe_unused]] static QXResult find_leaf_segment(
    QXSegmentHandle *segments,
    QXLeafHandle     leaf,
    QXSegmentHandle *out_segment
) noexcept {
    for (uint32_t i = 0u; i < QX_SEGMENT_COUNT; ++i) {
        QXSegmentHandle seg = segments[i];
        if (!seg) { continue; }

        // Probe each slot
        for (uint32_t s = 0u;
             s < QX_SLOTS_HIGH_PRIORITY; ++s)
        {
            QXLeafHandle found = nullptr;
            if (qx_segment_leaf_at_slot(seg, s, &found) == QX_OK
                && found == leaf)
            {
                *out_segment = seg;
                return QX_OK;
            }
        }
    }
    return QX_ERR_LEAF_NOT_FOUND;
}

/**
 * @brief Check Law 4 (Equilibrium) across all nine segments.
 *
 * A violation occurs when at least one segment is overloaded (> 80 %)
 * while at least one other segment is simultaneously starved (< 20 %).
 *
 * @param segments  Array of QX_SEGMENT_COUNT segment handles.
 * @param result    Receives equilibrium check result.
 */
static void check_equilibrium(
    QXSegmentHandle       *segments,
    QXEquilibriumResult   *result
) noexcept {
    std::memset(result, 0, sizeof(QXEquilibriumResult));

    uint32_t overloaded = 0u;
    uint32_t starved    = 0u;

    for (uint32_t i = 0u; i < QX_SEGMENT_COUNT; ++i) {
        QXSegmentHandle seg = segments[i];
        if (!seg) { continue; }

        QXSize used  = 0u;
        QXSize hard  = 0u;
        qx_segment_used_bytes(seg, &used);
        qx_segment_hard_limit(seg, &hard);

        double util = (hard > 0u)
                    ? static_cast<double>(used) /
                      static_cast<double>(hard) * 100.0
                    : 0.0;

        result->utilisation_pcts[i] = util;
        qx_segment_id(seg, result->segment_ids[i]);

        if (util >= QX_EQUILIBRIUM_OVERLOAD_PCT) { ++overloaded; }
        if (util <  QX_EQUILIBRIUM_STARVE_PCT
            && used > 0u)                         { ++starved;    }
    }

    result->overloaded_count = overloaded;
    result->starved_count    = starved;
    result->is_balanced      = (overloaded == 0u || starved == 0u)
                               ? QX_TRUE : QX_FALSE;
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXMemloc_s (internal representation)
// ─────────────────────────────────────────────────────────────────────────────

struct QXMemloc_s {
    // Nine segment handles matching kSegmentIds order
    QXSegmentHandle     segments[QX_SEGMENT_COUNT];

    // Reader-writer lock: multiple concurrent readers, exclusive writers
    mutable std::shared_mutex rwmtx;
};

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Public C ABI
// ─────────────────────────────────────────────────────────────────────────────

extern "C" {

// ── Lifecycle ─────────────────────────────────────────────────────────────────

QX_API QXResult qx_memloc_create(
    const QXSegmentConfig *configs,
    uint32_t               config_count,
    QXMemlocHandle        *out_memloc
) {
    if (!configs || !out_memloc)        { return QX_ERR_NULL_HANDLE;      }
    if (config_count != QX_SEGMENT_COUNT) { return QX_ERR_INVALID_ARGUMENT; }

    auto *mloc = new (std::nothrow) QXMemloc_s{};
    if (!mloc) { return QX_ERR_INTERNAL; }

    for (uint32_t i = 0u; i < QX_SEGMENT_COUNT; ++i) {
        mloc->segments[i] = nullptr;
    }

    for (uint32_t i = 0u; i < config_count; ++i) {
        const QXSegmentConfig &cfg = configs[i];

        // Validate segment ID
        const int idx =
            qx::internal::segment_index(cfg.segment_id);
        if (idx < 0) {
            // Cleanup previously created segments
            for (uint32_t j = 0u; j < i; ++j) {
                qx::internal::segment_destroy(mloc->segments[j]);
            }
            delete mloc;
            return QX_ERR_UNKNOWN_SEGMENT;
        }

        const uint32_t slot_count =
            qx::internal::segment_slot_count(
                static_cast<uint32_t>(idx));

        QXSegmentHandle seg = nullptr;
        const QXResult  r   = qx::internal::segment_create(
            cfg.segment_id,
            slot_count,
            cfg.effective_soft_bytes,
            cfg.effective_hard_bytes,
            &seg
        );
        if (r != QX_OK) {
            for (uint32_t j = 0u; j < i; ++j) {
                qx::internal::segment_destroy(mloc->segments[j]);
            }
            delete mloc;
            return r;
        }
        mloc->segments[static_cast<uint32_t>(idx)] = seg;
    }

    *out_memloc = mloc;
    return QX_OK;
}

QX_API void qx_memloc_destroy(QXMemlocHandle memloc) {
    if (!memloc) { return; }
    for (uint32_t i = 0u; i < QX_SEGMENT_COUNT; ++i) {
        qx::internal::segment_destroy(memloc->segments[i]);
    }
    delete memloc;
}

// ── Allocation ────────────────────────────────────────────────────────────────

QX_API QXResult qx_memloc_allocate(
    QXMemlocHandle    memloc,
    const char       *label,
    const char       *segment_id,
    QXLeafClassId     leaf_class,
    QXSize            size_bytes,
    QXAllocResult    *out_result
) {
    if (!memloc || !label || !segment_id || !out_result) {
        return QX_ERR_NULL_HANDLE;
    }
    if (size_bytes == 0u) { return QX_ERR_INVALID_ARGUMENT; }

    // Resolve segment
    const int idx = qx::internal::segment_index(segment_id);
    if (idx < 0) { return QX_ERR_UNKNOWN_SEGMENT; }

    std::unique_lock<std::shared_mutex> lock(memloc->rwmtx);
    QXSegmentHandle seg = memloc->segments[static_cast<uint32_t>(idx)];
    if (!seg) { return QX_ERR_UNKNOWN_SEGMENT; }

    // Law 2 – soft budget awareness (warning, not rejection)
    QXSize soft = 0u, hard = 0u, used = 0u;
    qx_segment_soft_limit(seg, &soft);
    qx_segment_hard_limit(seg, &hard);
    qx_segment_used_bytes(seg, &used);

    // Hard budget check performed inside segment_allocate
    // Soft budget breach is surfaced in out_result for caller telemetry

    // Create leaf (validates label – Law 1)
    QXLeafHandle leaf    = nullptr;
    uint32_t     slot    = 0u;
    QXResult     r       = qx::internal::leaf_create(
        label, segment_id, leaf_class, size_bytes, 0u, &leaf
    );
    if (r != QX_OK) { return r; }

    // Place leaf in segment slot
    r = qx::internal::segment_allocate(seg, leaf, size_bytes, &slot);
    if (r != QX_OK) {
        qx::internal::leaf_destroy(leaf);
        return r;
    }

    // Populate result
    std::memset(out_result, 0, sizeof(QXAllocResult));
    out_result->leaf         = leaf;
    out_result->slot_index   = slot;
    out_result->bytes_allocated = size_bytes;
    std::strncpy(out_result->segment_id, segment_id, 15u);
    out_result->segment_id[15] = '\0';

    // Soft pressure percentage
    const QXSize new_used = used + size_bytes;
    out_result->soft_pressure_pct =
        (soft > 0u)
        ? static_cast<double>(new_used) /
          static_cast<double>(soft) * 100.0
        : 0.0;

    // Pressure tier
    out_result->pressure_tier = QX_TIER_NORMAL;
    qx_segment_pressure_tier(seg, &out_result->pressure_tier);

    return QX_OK;
}

// ── Deallocation ──────────────────────────────────────────────────────────────

QX_API QXResult qx_memloc_deallocate(
    QXMemlocHandle  memloc,
    const char     *leaf_id,
    QXSize         *out_bytes
) {
    if (!memloc || !leaf_id) { return QX_ERR_NULL_HANDLE; }

    std::unique_lock<std::shared_mutex> lock(memloc->rwmtx);

    // Find leaf across all segments by scanning slot arrays
    for (uint32_t i = 0u; i < QX_SEGMENT_COUNT; ++i) {
        QXSegmentHandle seg = memloc->segments[i];
        if (!seg) { continue; }

        for (uint32_t s = 0u; s < QX_SLOTS_HIGH_PRIORITY; ++s) {
            QXLeafHandle lf = nullptr;
            if (qx_segment_leaf_at_slot(seg, s, &lf) != QX_OK
                || !lf) { continue; }

            // Match by leaf ID
            char id_buf[37] = { '\0' };
            qx_leaf_id(lf, id_buf);
            if (std::strcmp(id_buf, leaf_id) != 0) { continue; }

            // Guard: detect double free
            QXLeafState state = QX_LEAF_STATE_ACTIVE;
            qx_leaf_state(lf, &state);
            if (state == QX_LEAF_STATE_RELEASED) {
                return QX_ERR_DOUBLE_FREE;
            }

            // Transition leaf to RELEASED
            qx::internal::leaf_transition(
                lf, QX_LEAF_STATE_RELEASED, "explicit deallocation");

            // Remove from segment
            QXSize freed = 0u;
            qx::internal::segment_deallocate(seg, lf, &freed);
            if (out_bytes) { *out_bytes = freed; }

            // Destroy leaf
            qx::internal::leaf_destroy(lf);
            return QX_OK;
        }
    }
    return QX_ERR_LEAF_NOT_FOUND;
}

// ── Touch ─────────────────────────────────────────────────────────────────────

QX_API QXResult qx_memloc_touch(
    QXMemlocHandle memloc,
    const char    *leaf_id
) {
    if (!memloc || !leaf_id) { return QX_ERR_NULL_HANDLE; }

    std::shared_lock<std::shared_mutex> lock(memloc->rwmtx);

    for (uint32_t i = 0u; i < QX_SEGMENT_COUNT; ++i) {
        QXSegmentHandle seg = memloc->segments[i];
        if (!seg) { continue; }

        for (uint32_t s = 0u; s < QX_SLOTS_HIGH_PRIORITY; ++s) {
            QXLeafHandle lf = nullptr;
            if (qx_segment_leaf_at_slot(seg, s, &lf) != QX_OK
                || !lf) { continue; }

            char id_buf[37] = { '\0' };
            qx_leaf_id(lf, id_buf);
            if (std::strcmp(id_buf, leaf_id) != 0) { continue; }

            return qx_leaf_touch(lf);
        }
    }
    return QX_ERR_LEAF_NOT_FOUND;
}

// ── Stats ─────────────────────────────────────────────────────────────────────

QX_API QXResult qx_memloc_segment_stats(
    QXMemlocHandle  memloc,
    const char     *segment_id,
    QXSegmentStats *out_stats
) {
    if (!memloc || !segment_id || !out_stats) {
        return QX_ERR_NULL_HANDLE;
    }
    const int idx = qx::internal::segment_index(segment_id);
    if (idx < 0) { return QX_ERR_UNKNOWN_SEGMENT; }

    std::shared_lock<std::shared_mutex> lock(memloc->rwmtx);
    QXSegmentHandle seg = memloc->segments[static_cast<uint32_t>(idx)];
    if (!seg) { return QX_ERR_UNKNOWN_SEGMENT; }

    return qx_segment_stats(seg, out_stats);
}

QX_API QXResult qx_memloc_all_stats(
    QXMemlocHandle  memloc,
    QXSegmentStats *out_stats,
    uint32_t        count
) {
    if (!memloc || !out_stats)          { return QX_ERR_NULL_HANDLE;      }
    if (count < QX_SEGMENT_COUNT)       { return QX_ERR_INVALID_ARGUMENT; }

    std::shared_lock<std::shared_mutex> lock(memloc->rwmtx);

    for (uint32_t i = 0u; i < QX_SEGMENT_COUNT; ++i) {
        QXSegmentHandle seg = memloc->segments[i];
        if (!seg) { continue; }
        qx_segment_stats(seg, &out_stats[i]);
    }
    return QX_OK;
}

QX_API QXResult qx_memloc_total_used_bytes(
    QXMemlocHandle memloc,
    QXSize        *out_bytes
) {
    if (!memloc || !out_bytes) { return QX_ERR_NULL_HANDLE; }

    std::shared_lock<std::shared_mutex> lock(memloc->rwmtx);

    QXSize total = 0u;
    for (uint32_t i = 0u; i < QX_SEGMENT_COUNT; ++i) {
        QXSegmentHandle seg = memloc->segments[i];
        if (!seg) { continue; }
        QXSize used = 0u;
        qx_segment_used_bytes(seg, &used);
        total += used;
    }
    *out_bytes = total;
    return QX_OK;
}

QX_API QXResult qx_memloc_get_segment(
    QXMemlocHandle   memloc,
    const char      *segment_id,
    QXSegmentHandle *out_segment
) {
    if (!memloc || !segment_id || !out_segment) {
        return QX_ERR_NULL_HANDLE;
    }
    const int idx = qx::internal::segment_index(segment_id);
    if (idx < 0) { return QX_ERR_UNKNOWN_SEGMENT; }

    std::shared_lock<std::shared_mutex> lock(memloc->rwmtx);
    *out_segment = memloc->segments[static_cast<uint32_t>(idx)];
    return (*out_segment) ? QX_OK : QX_ERR_UNKNOWN_SEGMENT;
}

// ── Equilibrium ───────────────────────────────────────────────────────────────

QX_API QXResult qx_memloc_check_equilibrium(
    QXMemlocHandle       memloc,
    QXEquilibriumResult *out_result
) {
    if (!memloc || !out_result) { return QX_ERR_NULL_HANDLE; }

    std::shared_lock<std::shared_mutex> lock(memloc->rwmtx);
    check_equilibrium(memloc->segments, out_result);
    return QX_OK;
}

} // extern "C"
