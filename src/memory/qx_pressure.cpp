// =============================================================================
// QXEngine Core – src/memory/qx_pressure.cpp
// Owner : Masa Bayu
// Created: 2026-05-24
// Repo   : https://github.com/qxengine/qxengine-core
// Description: QXPressureCoordinator lifecycle, internal helpers, and
//              evaluation functions (create, destroy, evaluate, evaluate_all).
// =============================================================================

#include "qx_pressure_internal.h"

#include <cstdio>
#include <new>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

namespace {

/**
 * @brief Execute one eviction pass across all nine segments at a given tier.
 *
 * Collects candidates, transitions them to EVICTED, removes from segment,
 * and records leaf IDs in out_event (capped at QX_PRESSURE_EVENT_MAX_IDS).
 *
 * @param memloc       Non-NULL QXMemloc handle.
 * @param tier         Pressure tier to enforce (2–4).
 * @param out_event    Event struct to populate. May be NULL.
 * @param bytes_before Total used bytes before eviction.
 * @return QX_OK on success.
 */
static QXResult execute_eviction_pass(
    QXMemlocHandle    memloc,
    QXTierId          tier,
    QXPressureEvent  *out_event,
    QXSize            bytes_before
) noexcept {
    uint32_t total_evicted = 0u;
    QXSize   total_freed   = 0u;

    QXLeafHandle candidates[QX_SLOTS_HIGH_PRIORITY];

    for (uint32_t i = 0u; i < QX_SEGMENT_COUNT; ++i) {
        const char *seg_id = qx::internal::segment_id_at(i);
        if (!seg_id) { continue; }

        QXSegmentHandle seg = nullptr;
        if (qx_memloc_get_segment(memloc, seg_id, &seg) != QX_OK
            || !seg) { continue; }

        uint32_t candidate_count = 0u;
        std::memset(candidates, 0, sizeof(candidates));

        qx::internal::segment_eviction_candidates(
            seg, tier,
            candidates, QX_SLOTS_HIGH_PRIORITY,
            &candidate_count
        );

        for (uint32_t c = 0u; c < candidate_count; ++c) {
            QXLeafHandle lf = candidates[c];
            if (!lf) { continue; }

            char leaf_id[37] = { '\0' };
            qx_leaf_id(lf, leaf_id);

            qx::internal::leaf_transition(
                lf, QX_LEAF_STATE_EVICTED, "pressure eviction");

            QXSize freed = 0u;
            qx::internal::segment_evict(seg, lf, &freed);
            total_freed += freed;

            if (out_event &&
                total_evicted < QX_PRESSURE_EVENT_MAX_IDS)
            {
                std::strncpy(
                    out_event->evicted_ids[total_evicted],
                    leaf_id, 36u
                );
                out_event->evicted_ids[total_evicted][36] = '\0';
            }
            ++total_evicted;
        }
    }

    if (out_event) {
        out_event->trigger_tier    = tier;
        out_event->bytes_before    = bytes_before;
        out_event->bytes_after     = (bytes_before >= total_freed)
                                     ? bytes_before - total_freed : 0u;
        out_event->bytes_reclaimed = total_freed;
        out_event->evicted_count   = total_evicted;
        out_event->timestamp_ms    = qx_pressure_now_ms();

        std::snprintf(
            out_event->summary,
            sizeof(out_event->summary),
            "Tier %u eviction – %u leaves evicted, %llu bytes freed",
            static_cast<unsigned>(tier),
            total_evicted,
            static_cast<unsigned long long>(total_freed)
        );
    }

    return QX_OK;
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Public C ABI – Lifecycle & Evaluation
// ─────────────────────────────────────────────────────────────────────────────

extern "C" {

QX_API QXResult qx_pressure_create(
    QXMemlocHandle                memloc,
    QXPressureCoordinatorHandle  *out_pressure
) {
    if (!memloc || !out_pressure) { return QX_ERR_NULL_HANDLE; }

    auto *coord = new (std::nothrow) QXPressureCoordinator_s{};
    if (!coord) { return QX_ERR_INTERNAL; }

    coord->memloc                 = memloc;
    coord->history_count          = 0u;
    coord->history_head           = 0u;
    coord->resident_bytes         = 0u;
    coord->system_total_bytes     = 0u;
    coord->system_avail_bytes     = 0u;
    coord->peak_tier              = QX_TIER_NORMAL;
    coord->total_evaluations      = 0u;
    coord->eviction_cycles        = 0u;
    coord->total_bytes_reclaimed  = 0u;
    coord->total_leaves_evicted   = 0u;
    coord->last_evaluated_ms      = 0u;
    coord->last_pressure_event_ms = 0u;

    std::memset(coord->history, 0,
                sizeof(QXPressureEvent) * QX_PRESSURE_EVENT_HISTORY_CAP);

    *out_pressure = coord;
    return QX_OK;
}

QX_API void qx_pressure_destroy(
    QXPressureCoordinatorHandle pressure
) {
    delete pressure;
}

QX_API QXResult qx_pressure_evaluate(
    QXPressureCoordinatorHandle  pressure,
    QXPressureEvent             *out_event
) {
    if (!pressure) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(pressure->mtx);

    ++pressure->total_evaluations;
    pressure->last_evaluated_ms = qx_pressure_now_ms();

    const QXTierId tier = qx_pressure_composite_tier(pressure->memloc);
    if (tier > pressure->peak_tier) { pressure->peak_tier = tier; }

    // Tier 1 – no eviction required
    if (tier == QX_TIER_NORMAL) { return QX_OK; }

    QXSize bytes_before = 0u;
    qx_memloc_total_used_bytes(pressure->memloc, &bytes_before);

    QXPressureEvent event{};
    std::memset(&event, 0, sizeof(event));

    const QXResult r =
        execute_eviction_pass(pressure->memloc, tier, &event, bytes_before);
    if (r != QX_OK) { return r; }

    pressure->total_bytes_reclaimed  += event.bytes_reclaimed;
    pressure->total_leaves_evicted   += event.evicted_count;
    pressure->last_pressure_event_ms  = event.timestamp_ms;
    ++pressure->eviction_cycles;

    qx_pressure_append_event(pressure, event);
    if (out_event) { *out_event = event; }

    const QXTierId post_tier = qx_pressure_composite_tier(pressure->memloc);
    if (post_tier >= QX_TIER_CRITICAL && tier >= QX_TIER_CRITICAL
        && event.evicted_count == 0u)
    {
        return QX_ERR_INSUFFICIENT_EVICTION;
    }

    return QX_OK;
}

QX_API QXResult qx_pressure_evaluate_all(
    QXPressureCoordinatorHandle  pressure,
    QXTierId                     forced_tier,
    QXPressureEvent             *out_event
) {
    if (!pressure) { return QX_ERR_NULL_HANDLE; }
    if (forced_tier < QX_TIER_ELEVATED ||
        forced_tier > QX_TIER_CRITICAL)
    {
        return QX_ERR_INVALID_ARGUMENT;
    }
    std::lock_guard<std::mutex> lock(pressure->mtx);

    ++pressure->total_evaluations;
    pressure->last_evaluated_ms = qx_pressure_now_ms();
    if (forced_tier > pressure->peak_tier) {
        pressure->peak_tier = forced_tier;
    }

    QXSize bytes_before = 0u;
    qx_memloc_total_used_bytes(pressure->memloc, &bytes_before);

    QXPressureEvent event{};
    std::memset(&event, 0, sizeof(event));

    const QXResult r = execute_eviction_pass(
        pressure->memloc, forced_tier, &event, bytes_before);
    if (r != QX_OK) { return r; }

    pressure->total_bytes_reclaimed  += event.bytes_reclaimed;
    pressure->total_leaves_evicted   += event.evicted_count;
    pressure->last_pressure_event_ms  = event.timestamp_ms;
    ++pressure->eviction_cycles;

    qx_pressure_append_event(pressure, event);
    if (out_event) { *out_event = event; }

    if (forced_tier >= QX_TIER_CRITICAL && event.evicted_count == 0u) {
        return QX_ERR_INSUFFICIENT_EVICTION;
    }
    return QX_OK;
}

} // extern "C"
