// =============================================================================
// QXEngine Core – src/memory/qx_pressure_internal.h
// Owner : Masa Bayu
// Created: 2026-05-24
// Repo   : https://github.com/qxengine/qxengine-core
// Description: Private internal header shared between qx_pressure.cpp and
//              qx_pressure_api.cpp. Exposes QXPressureCoordinator_s,
//              append_event, now_ms, and composite_tier.
//              NOT part of the public install interface.
// =============================================================================

#ifndef QX_PRESSURE_INTERNAL_H
#define QX_PRESSURE_INTERNAL_H

#include "qxengine/memory/qx_pressure.h"
#include "qxengine/memory/qx_memloc.h"
#include "qxengine/memory/qx_segment.h"
#include "qxengine/memory/qx_leaf.h"
#include "qxengine/core/qx_constants.h"
#include "qxengine/core/qx_error.h"
#include "qxengine/core/qx_types.h"

#include <cstring>
#include <mutex>
#include <chrono>

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – qx::internal forward declarations
// ─────────────────────────────────────────────────────────────────────────────

namespace qx::internal {
    QXResult    leaf_transition(QXLeafHandle, QXLeafState,
                                const char*) noexcept;
    QXResult    segment_evict(QXSegmentHandle, QXLeafHandle,
                              QXSize*) noexcept;
    QXResult    segment_eviction_candidates(QXSegmentHandle, QXTierId,
                                            QXLeafHandle*, uint32_t,
                                            uint32_t*) noexcept;
    int         segment_index(const char*) noexcept;
    const char *segment_id_at(uint32_t) noexcept;
} // namespace qx::internal

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXPressureCoordinator_s
// ─────────────────────────────────────────────────────────────────────────────

struct QXPressureCoordinator_s {
    // Bound memloc (not owned)
    QXMemlocHandle  memloc;

    // Event history ring buffer
    QXPressureEvent history[QX_PRESSURE_EVENT_HISTORY_CAP];
    uint32_t        history_count;
    uint32_t        history_head;       // next write position

    // OS memory feed
    QXSize          resident_bytes;
    QXSize          system_total_bytes;
    QXSize          system_avail_bytes;

    // Aggregate stats
    QXTierId        peak_tier;
    uint64_t        total_evaluations;
    uint64_t        eviction_cycles;
    QXSize          total_bytes_reclaimed;
    uint64_t        total_leaves_evicted;
    QXTimestamp     last_evaluated_ms;
    QXTimestamp     last_pressure_event_ms;

    // Thread safety
    mutable std::mutex mtx;
};

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Shared inline helpers
// ─────────────────────────────────────────────────────────────────────────────

static inline QXTimestamp qx_pressure_now_ms() noexcept {
    using namespace std::chrono;
    return static_cast<QXTimestamp>(
        duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()
        ).count()
    );
}

static inline void qx_pressure_append_event(
    QXPressureCoordinator_s *coord,
    const QXPressureEvent   &event
) noexcept {
    coord->history[coord->history_head] = event;
    coord->history_head =
        (coord->history_head + 1u) % QX_PRESSURE_EVENT_HISTORY_CAP;
    if (coord->history_count < QX_PRESSURE_EVENT_HISTORY_CAP) {
        ++coord->history_count;
    }
}

static inline QXTierId qx_pressure_composite_tier(
    QXMemlocHandle memloc
) noexcept {
    QXTierId max_tier = QX_TIER_NORMAL;
    for (uint32_t i = 0u; i < QX_SEGMENT_COUNT; ++i) {
        const char *seg_id = qx::internal::segment_id_at(i);
        if (!seg_id) { continue; }
        QXSegmentHandle seg = nullptr;
        if (qx_memloc_get_segment(memloc, seg_id, &seg) != QX_OK
            || !seg) { continue; }
        QXTierId tier = QX_TIER_NORMAL;
        qx_segment_pressure_tier(seg, &tier);
        if (tier > max_tier) { max_tier = tier; }
    }
    return max_tier;
}

#endif /* QX_PRESSURE_INTERNAL_H */
