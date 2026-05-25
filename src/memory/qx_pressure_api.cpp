// =============================================================================
// QXEngine Core – src/memory/qx_pressure_api.cpp
// Owner : Masa Bayu
// Created: 2026-05-24
// Repo   : https://github.com/qxengine/qxengine-core
// Description: Public C ABI for QXPressureCoordinator – OS signal handlers,
//              tier queries, event history access, and statistics.
// =============================================================================

#include "qx_pressure_internal.h"

#include <cstring>
#include <algorithm>
#include <mutex>

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Public C ABI – OS Signals
// ─────────────────────────────────────────────────────────────────────────────

extern "C" {

QX_API QXResult qx_pressure_handle_android_trim(
    QXPressureCoordinatorHandle  pressure,
    int32_t                      android_trim_level,
    QXPressureEvent             *out_event
) {
    if (!pressure) { return QX_ERR_NULL_HANDLE; }

    QXTierId tier = QX_TIER_ELEVATED;
    switch (android_trim_level) {
        case  5: tier = QX_TIER_ELEVATED; break;
        case 10: tier = QX_TIER_HIGH;     break;
        case 15: tier = QX_TIER_CRITICAL; break;
        case 20: tier = QX_TIER_ELEVATED; break;
        case 40: tier = QX_TIER_ELEVATED; break;
        case 60: tier = QX_TIER_HIGH;     break;
        case 80: tier = QX_TIER_CRITICAL; break;
        default: tier = QX_TIER_ELEVATED; break;
    }

    return qx_pressure_evaluate_all(pressure, tier, out_event);
}

QX_API QXResult qx_pressure_handle_ios_warning(
    QXPressureCoordinatorHandle  pressure,
    QXPressureEvent             *out_event
) {
    if (!pressure) { return QX_ERR_NULL_HANDLE; }

    // Default: Tier 3. Escalate to Tier 4 if resident ≥ 80 % of total RAM.
    QXTierId tier = QX_TIER_HIGH;
    {
        std::lock_guard<std::mutex> lock(pressure->mtx);
        if (pressure->system_total_bytes > 0u) {
            const double pct =
                static_cast<double>(pressure->resident_bytes) /
                static_cast<double>(pressure->system_total_bytes);
            if (pct >= 0.80) { tier = QX_TIER_CRITICAL; }
        }
    }
    return qx_pressure_evaluate_all(pressure, tier, out_event);
}

QX_API QXResult qx_pressure_feed_memory(
    QXPressureCoordinatorHandle  pressure,
    QXSize                       resident_bytes,
    QXSize                       system_total_bytes,
    QXSize                       system_avail_bytes
) {
    if (!pressure)               { return QX_ERR_NULL_HANDLE;      }
    if (system_total_bytes == 0u){ return QX_ERR_INVALID_ARGUMENT; }

    std::lock_guard<std::mutex> lock(pressure->mtx);
    pressure->resident_bytes     = resident_bytes;
    pressure->system_total_bytes = system_total_bytes;
    pressure->system_avail_bytes = system_avail_bytes;
    return QX_OK;
}

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Tier Queries
// ─────────────────────────────────────────────────────────────────────────────

QX_API QXResult qx_pressure_current_tier(
    QXPressureCoordinatorHandle  pressure,
    QXTierId                    *out_tier
) {
    if (!pressure || !out_tier) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(pressure->mtx);
    *out_tier = qx_pressure_composite_tier(pressure->memloc);
    return QX_OK;
}

QX_API QXResult qx_pressure_segment_tier(
    QXPressureCoordinatorHandle  pressure,
    const char                  *segment_id,
    QXTierId                    *out_tier
) {
    if (!pressure || !segment_id || !out_tier) {
        return QX_ERR_NULL_HANDLE;
    }
    std::lock_guard<std::mutex> lock(pressure->mtx);

    QXSegmentHandle seg = nullptr;
    const QXResult r =
        qx_memloc_get_segment(pressure->memloc, segment_id, &seg);
    if (r != QX_OK || !seg) { return QX_ERR_UNKNOWN_SEGMENT; }

    return qx_segment_pressure_tier(seg, out_tier);
}

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Event History
// ─────────────────────────────────────────────────────────────────────────────

QX_API QXResult qx_pressure_event_count(
    QXPressureCoordinatorHandle  pressure,
    uint32_t                    *out_count
) {
    if (!pressure || !out_count) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(pressure->mtx);
    *out_count = pressure->history_count;
    return QX_OK;
}

QX_API QXResult qx_pressure_event_at(
    QXPressureCoordinatorHandle  pressure,
    uint32_t                     index,
    QXPressureEvent             *out_event
) {
    if (!pressure || !out_event) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(pressure->mtx);

    if (index >= pressure->history_count) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    const uint32_t cap     = QX_PRESSURE_EVENT_HISTORY_CAP;
    const uint32_t cnt     = pressure->history_count;
    const uint32_t head    = pressure->history_head;
    const uint32_t oldest  = (cnt < cap) ? 0u : head;
    const uint32_t physical = (oldest + index) % cap;

    *out_event = pressure->history[physical];
    return QX_OK;
}

QX_API QXResult qx_pressure_events_copy(
    QXPressureCoordinatorHandle  pressure,
    QXPressureEvent             *out_events,
    uint32_t                     capacity,
    uint32_t                    *out_written
) {
    if (!pressure || !out_events || !out_written) {
        return QX_ERR_NULL_HANDLE;
    }
    if (capacity == 0u) { return QX_ERR_INVALID_ARGUMENT; }
    std::lock_guard<std::mutex> lock(pressure->mtx);

    const uint32_t to_copy =
        std::min(capacity, pressure->history_count);

    const uint32_t cap    = QX_PRESSURE_EVENT_HISTORY_CAP;
    const uint32_t cnt    = pressure->history_count;
    const uint32_t head   = pressure->history_head;
    const uint32_t oldest = (cnt < cap) ? 0u : head;

    for (uint32_t i = 0u; i < to_copy; ++i) {
        const uint32_t physical = (oldest + i) % cap;
        out_events[i] = pressure->history[physical];
    }

    *out_written = to_copy;
    return QX_OK;
}

QX_API QXResult qx_pressure_clear_history(
    QXPressureCoordinatorHandle pressure
) {
    if (!pressure) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(pressure->mtx);
    pressure->history_count = 0u;
    pressure->history_head  = 0u;
    std::memset(pressure->history, 0,
                sizeof(QXPressureEvent) * QX_PRESSURE_EVENT_HISTORY_CAP);
    return QX_OK;
}

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Statistics
// ─────────────────────────────────────────────────────────────────────────────

QX_API QXResult qx_pressure_stats(
    QXPressureCoordinatorHandle  pressure,
    QXPressureStats             *out_stats
) {
    if (!pressure || !out_stats) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(pressure->mtx);

    out_stats->current_tier           =
        qx_pressure_composite_tier(pressure->memloc);
    out_stats->peak_tier              = pressure->peak_tier;
    out_stats->total_evaluations      = pressure->total_evaluations;
    out_stats->eviction_cycles        = pressure->eviction_cycles;
    out_stats->total_bytes_reclaimed  = pressure->total_bytes_reclaimed;
    out_stats->total_leaves_evicted   = pressure->total_leaves_evicted;
    out_stats->event_history_count    = pressure->history_count;
    out_stats->last_evaluated_ms      = pressure->last_evaluated_ms;
    out_stats->last_pressure_event_ms = pressure->last_pressure_event_ms;

    return QX_OK;
}

} // extern "C"
