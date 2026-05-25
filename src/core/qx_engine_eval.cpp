// =============================================================================
// QXEngine Core – src/core/qx_engine_eval.cpp
// Owner      : Masa Bayu
// Created    : 2026-05-25
// Repository : https://github.com/qxengine/qxengine-core
// Purpose    : Constitutional evaluation and cognitive snapshot capture.
// =============================================================================

#include "qx_engine_internal.h"

extern "C" {

QXResult qx_engine_evaluate_now(QXEngineHandle engine)
{
    if (!engine) return QX_ERR_NULL_HANDLE;
    {
        std::shared_lock lk(engine->state_mtx);
        if (engine->state != QX_ENGINE_STATE_RUNNING) return QX_ERR_NOT_READY;
    }

    std::lock_guard<std::mutex> eval_lk(engine->eval_mtx);

    QXLawEvaluationInput input{};
    qx_law_input_zero_init(&input);
    input.captured_at_ms = qx_engine_now_ms();

    input.unlabelled_allocation_count =
        engine->unlabelled_alloc_count.exchange(0u,
                                                 std::memory_order_acq_rel);

    QXSegmentStats ss[QX_SEGMENT_COUNT]{};
    if (qx_memloc_all_stats(engine->memloc, ss, QX_SEGMENT_COUNT) == QX_OK) {
        input.segment_count = QX_SEGMENT_COUNT;
        for (uint32_t i = 0u; i < QX_SEGMENT_COUNT; ++i) {
            qx_law_segment_input_from_stats(&ss[i], &input.segments[i]);
        }
    } else {
        input.segment_count = QX_SEGMENT_COUNT;
    }

    const QXTimestamp snap_ms = engine->last_snapshot_ms.load(
                                    std::memory_order_acquire);
    const QXTimestamp now     = input.captured_at_ms;
    input.knowledge_score     = engine->knowledge_score;
    input.seconds_since_last_snapshot =
        (snap_ms > 0u)
            ? static_cast<double>(now - snap_ms) / 1000.0
            : static_cast<double>(QX_SNAPSHOT_INTERVAL_SEC + 1u);

    input.dark_patterns_prohibited   = engine->ethics_dark_patterns;
    input.deceptive_flows_prohibited = engine->ethics_deceptive_flows;
    input.manipulative_ux_prohibited = engine->ethics_manipulative_ux;
    input.privacy_first_design       = engine->ethics_privacy_first;
    input.transparent_data_usage     = engine->ethics_transparent_data;

    input.native_first_policy = engine->native_first_policy;
    {
        std::lock_guard<std::mutex> cap_lk(engine->cap_mtx);
        input.declared_capability_count = engine->capabilities.declared_count;
        input.active_capability_count   = engine->capabilities.active_count;
        input.native_utilisation_ratio  = engine->capabilities.utilisation_ratio();
        std::memcpy(input.declared_capabilities,
                    engine->capabilities.declared,
                    sizeof(input.declared_capabilities));
        std::memcpy(input.active_capabilities,
                    engine->capabilities.active,
                    sizeof(input.active_capabilities));
    }

    {
        std::lock_guard<std::mutex> eco_lk(engine->economy_mtx);
        input.battery_drain_pct_per_10min = engine->battery_drain_pct;
        input.network_redundancy_pct      = engine->network_redundancy_pct;
    }

    QXLawReport report{};
    QXResult rc = qx_law_enforcer_evaluate(engine->enforcer, &input, &report);
    if (rc != QX_OK) return rc;

    rc = qx_bridge_run_cycle(&engine->memloc_bridge, now, &report);
    if (rc != QX_OK) return rc;

    engine->knowledge_score = report.health_score;
    engine->eval_count.fetch_add(1u, std::memory_order_relaxed);

    QXSnapshotInput snap_input{};
    snap_input.captured_at_ms          = now;
    snap_input.latest_law_health_score = report.health_score;
    snap_input.total_used_bytes        = 0u;
    snap_input.composite_pressure_tier = QX_TIER_NORMAL;

    {
        QXSegmentStats ss2[QX_SEGMENT_COUNT]{};
        if (qx_memloc_all_stats(engine->memloc, ss2, QX_SEGMENT_COUNT) == QX_OK) {
            snap_input.segment_count = QX_SEGMENT_COUNT;
            for (uint32_t i = 0u; i < QX_SEGMENT_COUNT; ++i) {
                std::strncpy(snap_input.segment_ids[i], ss2[i].segment_id,
                             sizeof(snap_input.segment_ids[i]) - 1u);
                snap_input.segment_utilisations[i] =
                    ss2[i].hard_limit_bytes > 0u
                        ? static_cast<double>(ss2[i].used_bytes)
                          / static_cast<double>(ss2[i].hard_limit_bytes)
                        : 0.0;
                snap_input.total_used_bytes += ss2[i].used_bytes;
            }
        } else {
            snap_input.segment_count = QX_SEGMENT_COUNT;
        }
    }

    {
        uint32_t streak    = 0u;
        uint32_t rep_count = 0u;
        if (qx_law_enforcer_report_count(engine->enforcer, &rep_count) == QX_OK) {
            for (uint32_t i = rep_count; i > 0u; --i) {
                QXLawReport hist_rep{};
                if (qx_law_enforcer_report_at(engine->enforcer,
                                               i - 1u, &hist_rep) != QX_OK) break;
                if (!hist_rep.is_fully_compliant) break;
                ++streak;
            }
        }
        snap_input.compliance_streak = streak;
    }

    snap_input.native_coverage_ratio     = engine->capabilities.utilisation_ratio();
    snap_input.declared_capability_count = engine->capabilities.declared_count;
    snap_input.active_capability_count   = engine->capabilities.active_count;

    QXCognitiveSnapshot snap{};
    qx_snapshot_capture(engine->snapshots, &snap_input, &snap);
    engine->last_snapshot_ms.store(now, std::memory_order_release);
    engine->snapshot_count.fetch_add(1u, std::memory_order_relaxed);

    engine->last_report       = report;
    engine->last_report_valid = true;
    engine->last_eval_ms      = now;
    qx_telemetry_record_report(engine->telemetry, &report, nullptr);

    return QX_OK;
}

} // extern "C"
