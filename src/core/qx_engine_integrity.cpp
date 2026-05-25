// =============================================================================
// QXEngine Core – src/core/qx_engine_integrity.cpp
// Owner      : Masa Bayu
// Created    : 2026-05-25
// Repository : https://github.com/qxengine/qxengine-core
// Purpose    : Runtime feeds, Law 7/8 updates, and integrity reporting.
// =============================================================================

#include "qx_engine_internal.h"

extern "C" {

QXResult qx_engine_notify_memory_pressure(QXEngineHandle engine,
                                            uint8_t        pressure_level)
{
    if (!engine) return QX_ERR_NULL_HANDLE;
    if (pressure_level < QX_TIER_NORMAL ||
        pressure_level > QX_TIER_CRITICAL) return QX_ERR_INVALID_ARGUMENT;

    QXPressureEvent pev{};
    const auto tier = static_cast<QXTierId>(pressure_level);
    qx_bridge_notify_pressure(&engine->memloc_bridge, pressure_level);
    if (tier >= QX_TIER_ELEVATED) {
        return qx_pressure_evaluate_all(engine->pressure, tier, &pev);
    }
    return qx_pressure_evaluate(engine->pressure, &pev);
}

QXResult qx_engine_update_battery_drain(QXEngineHandle engine,
                                          double         drain_pct)
{
    if (!engine) return QX_ERR_NULL_HANDLE;
    if (drain_pct < 0.0 || drain_pct > 100.0) return QX_ERR_INVALID_ARGUMENT;

    {
        std::lock_guard<std::mutex> lk(engine->economy_mtx);
        engine->battery_drain_pct = drain_pct;
    }

    QXTelemetryEventInput ev{};
    ev.category    = QX_TEL_ENGINE;
    ev.result      = QX_OK;
    ev.health_score = drain_pct;
    std::strncpy(ev.message, "battery_drain_pct", sizeof(ev.message) - 1u);
    qx_telemetry_record(engine->telemetry, &ev, nullptr);
    return QX_OK;
}

QXResult qx_engine_update_network_redundancy(QXEngineHandle engine,
                                               double         redundancy_pct)
{
    if (!engine) return QX_ERR_NULL_HANDLE;
    if (redundancy_pct < 0.0 || redundancy_pct > 200.0)
        return QX_ERR_INVALID_ARGUMENT;

    {
        std::lock_guard<std::mutex> lk(engine->economy_mtx);
        engine->network_redundancy_pct = redundancy_pct;
    }

    QXTelemetryEventInput ev{};
    ev.category    = QX_TEL_ENGINE;
    ev.result      = QX_OK;
    ev.health_score = redundancy_pct;
    std::strncpy(ev.message, "network_redundancy_pct", sizeof(ev.message) - 1u);
    qx_telemetry_record(engine->telemetry, &ev, nullptr);
    return QX_OK;
}

QXResult qx_engine_feed_memory_reading(QXEngineHandle engine,
                                         uint64_t       resident_bytes,
                                         uint64_t       system_total,
                                         uint64_t       system_avail)
{
    if (!engine) return QX_ERR_NULL_HANDLE;
    QXResult rc = qx_pressure_feed_memory(engine->pressure,
                                          static_cast<QXSize>(resident_bytes),
                                          static_cast<QXSize>(system_total),
                                          static_cast<QXSize>(system_avail));
    if (rc == QX_OK && system_total > 0u) {
        const uint64_t available = (system_avail > system_total)
            ? system_total
            : system_avail;
        const double used = static_cast<double>(system_total - available)
                          / static_cast<double>(system_total);
        uint8_t tier = QX_TIER_NORMAL;
        if (used >= 0.95) {
            tier = QX_TIER_CRITICAL;
        } else if (used >= 0.85) {
            tier = QX_TIER_HIGH;
        } else if (used >= 0.70) {
            tier = QX_TIER_ELEVATED;
        }
        qx_bridge_notify_pressure(&engine->memloc_bridge, tier);
    }
    return rc;
}

QXResult qx_engine_notify_lifecycle_change(QXEngineHandle engine,
                                           uint8_t lifecycle_state)
{
    if (!engine) return QX_ERR_NULL_HANDLE;
    if (lifecycle_state > 1u) return QX_ERR_INVALID_ARGUMENT;
    return qx_bridge_notify_lifecycle(&engine->memloc_bridge, lifecycle_state);
}

QXResult qx_engine_notify_screen_profile_change(QXEngineHandle engine,
                                                const char* screen_profile_id)
{
    if (!engine || !screen_profile_id) return QX_ERR_NULL_HANDLE;
    return qx_bridge_notify_screen(&engine->memloc_bridge, screen_profile_id);
}

QXResult qx_engine_mark_capability_active(QXEngineHandle engine,
                                            const char*    capability_id)
{
    if (!engine || !capability_id) return QX_ERR_NULL_HANDLE;

    {
        std::lock_guard<std::mutex> lk(engine->cap_mtx);
        engine->capabilities.mark_active(capability_id);
    }

    QXTelemetryEventInput ev{};
    ev.category = QX_TEL_NATIVE;
    ev.result   = QX_OK;
    std::strncpy(ev.message, capability_id, sizeof(ev.message) - 1u);
    qx_telemetry_record(engine->telemetry, &ev, nullptr);
    return QX_OK;
}

QXResult qx_engine_integrity_report(QXEngineHandle engine,
                                     QXIntegrityReport* out_report)
{
    if (!engine || !out_report) return QX_ERR_NULL_HANDLE;

    QXIntegrityReport report{};
    report.evaluated_at_ms = qx_engine_now_ms();

    QXEngineState state = QX_ENGINE_STATE_UNCONFIGURED;
    {
        std::shared_lock lk(engine->state_mtx);
        state = engine->state;
    }

    const uint64_t eval_count =
        engine->eval_count.load(std::memory_order_acquire);
    const uint64_t snap_count =
        engine->snapshot_count.load(std::memory_order_acquire);

    bool last_valid = false;
    QXLawReport last{};
    {
        std::lock_guard<std::mutex> lk(engine->eval_mtx);
        last_valid = engine->last_report_valid;
        last = engine->last_report;
    }

    uint32_t declared_caps = 0u;
    uint32_t active_caps = 0u;
    {
        std::lock_guard<std::mutex> lk(engine->cap_mtx);
        declared_caps = engine->capabilities.declared_count;
        active_caps = engine->capabilities.active_count;
    }

    report.evaluation_count = eval_count;
    report.snapshot_count = snap_count;
    report.health_score = last_valid ? last.health_score : 0.0;

    report.stage_passed[QX_INTEGRITY_ACCEPTANCE] =
        (engine->manifest && state != QX_ENGINE_STATE_UNCONFIGURED)
            ? QX_TRUE : QX_FALSE;

    report.stage_passed[QX_INTEGRITY_IMPLEMENTATION] =
        (engine->memloc && engine->pressure && engine->enforcer &&
         engine->snapshots && engine->telemetry)
            ? QX_TRUE : QX_FALSE;

    report.stage_passed[QX_INTEGRITY_PATIENCE] =
        (eval_count > 0u && snap_count > 0u) ? QX_TRUE : QX_FALSE;

    report.stage_passed[QX_INTEGRITY_REFINEMENT] =
        (last_valid && last.has_fatal_violation == QX_FALSE)
            ? QX_TRUE : QX_FALSE;

    report.stage_passed[QX_INTEGRITY_ACCURACY] =
        (last_valid && last.health_score >= 0.0 && last.health_score <= 100.0)
            ? QX_TRUE : QX_FALSE;

    report.stage_passed[QX_INTEGRITY_TRANQUILITY] =
        (last_valid && last.has_fatal_violation == QX_FALSE &&
         last.has_critical_violation == QX_FALSE && last.health_score >= 60.0)
            ? QX_TRUE : QX_FALSE;

    report.stage_passed[QX_INTEGRITY_SINCERITY] =
        (report.stage_passed[QX_INTEGRITY_TRANQUILITY] == QX_TRUE &&
         (declared_caps == 0u || active_caps == declared_caps))
            ? QX_TRUE : QX_FALSE;

    report.status = QX_INTEGRITY_STATUS_OK;
    report.current_stage = QX_INTEGRITY_SINCERITY;
    const char* reason = "integrity ok";

    for (uint32_t i = 0u; i < QX_INTEGRITY_STAGE_COUNT; ++i) {
        if (report.stage_passed[i] == QX_FALSE) {
            report.current_stage = static_cast<QXIntegrityStage>(i);
            if (i <= QX_INTEGRITY_PATIENCE) {
                report.status = QX_INTEGRITY_STATUS_PENDING;
            } else {
                report.status = QX_INTEGRITY_STATUS_NOT_OK;
            }
            static const char* const reasons[QX_INTEGRITY_STAGE_COUNT] = {
                "manifest acceptance is incomplete",
                "runtime implementation is incomplete",
                "observation window is incomplete",
                "fatal violation requires refinement",
                "integrity evidence is not accurate",
                "runtime is not tranquil",
                "declaration and runtime behavior are not sincere"
            };
            reason = reasons[i];
            break;
        }
    }

    std::strncpy(report.failure_reason, reason,
                 sizeof(report.failure_reason) - 1u);
    *out_report = report;
    return QX_OK;
}

QXResult qx_engine_integrity_status(QXEngineHandle engine,
                                     QXIntegrityStatus* out_status)
{
    if (!engine || !out_status) return QX_ERR_NULL_HANDLE;
    QXIntegrityReport report{};
    const QXResult rc = qx_engine_integrity_report(engine, &report);
    if (rc != QX_OK) return rc;
    *out_status = report.status;
    return QX_OK;
}

} // extern "C"
