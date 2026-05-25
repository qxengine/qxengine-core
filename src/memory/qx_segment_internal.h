// =============================================================================
// QXEngine Core – src/memory/qx_segment_internal.h
// Owner : Masa Bayu
// Created: 2026-05-24
// Repo   : https://github.com/qxengine/qxengine-core
// Description: Private internal header shared between qx_segment.cpp and
//              qx_segment_api.cpp. Exposes QXSegment_s definition and the
//              qx::internal factory/mutation functions.
//              NOT part of the public install interface.
// =============================================================================

#ifndef QX_SEGMENT_INTERNAL_H
#define QX_SEGMENT_INTERNAL_H

#include "qxengine/memory/qx_segment.h"
#include "qxengine/memory/qx_leaf.h"
#include "qxengine/core/qx_constants.h"
#include "qxengine/core/qx_error.h"
#include "qxengine/core/qx_types.h"

#include <mutex>

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXSegment_s
// ─────────────────────────────────────────────────────────────────────────────

struct QXSegment_s {
    // ── Identity (immutable after creation) ───────────────────────────────────
    char        segment_id[16];
    uint32_t    max_slots;
    QXSize      soft_limit_bytes;
    QXSize      hard_limit_bytes;

    // ── Slot array ────────────────────────────────────────────────────────────
    QXLeafHandle slots[QX_SLOTS_HIGH_PRIORITY]; // max 7 slots per segment

    // ── Mutable counters ──────────────────────────────────────────────────────
    QXSize      used_bytes;
    uint32_t    active_count;
    uint32_t    evicted_count;
    uint32_t    released_count;
    uint64_t    total_allocations;
    uint64_t    total_deallocations;
    uint64_t    total_evictions;

    // ── Thread safety ─────────────────────────────────────────────────────────
    mutable std::mutex mtx;
};

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Internal timestamp helper (shared)
// ─────────────────────────────────────────────────────────────────────────────

#include <chrono>

static inline QXTimestamp qx_seg_now_ms() noexcept {
    using namespace std::chrono;
    return static_cast<QXTimestamp>(
        duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()
        ).count()
    );
}

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – qx::internal function declarations
// ─────────────────────────────────────────────────────────────────────────────

namespace qx::internal {

QXResult segment_create(
    const char       *segment_id,
    uint32_t          max_slots,
    QXSize            soft_limit_bytes,
    QXSize            hard_limit_bytes,
    QXSegmentHandle  *out_segment
) noexcept;

void segment_destroy(QXSegmentHandle segment) noexcept;

QXResult segment_allocate(
    QXSegmentHandle segment,
    QXLeafHandle    leaf,
    QXSize          size_bytes,
    uint32_t       *out_slot
) noexcept;

QXResult segment_deallocate(
    QXSegmentHandle segment,
    QXLeafHandle    leaf,
    QXSize         *out_bytes
) noexcept;

QXResult segment_evict(
    QXSegmentHandle segment,
    QXLeafHandle    leaf,
    QXSize         *out_bytes
) noexcept;

QXResult segment_eviction_candidates(
    QXSegmentHandle segment,
    QXTierId        tier,
    QXLeafHandle   *out_leaves,
    uint32_t        capacity,
    uint32_t       *out_count
) noexcept;

} // namespace qx::internal

#endif /* QX_SEGMENT_INTERNAL_H */
