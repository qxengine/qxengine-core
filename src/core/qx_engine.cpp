// =============================================================================
// src/core/qx_engine.cpp
// QXEngine Core – Top-level engine lifecycle and allocation orchestration.
//
// Owner      : Masa Bayu
// Created    : 2026-05-24
// Revised    : 2026-05-25  – Split evaluation and integrity/runtime feeds into
//                            focused implementation units.
// Repository : https://github.com/qxengine/qxengine-core
// =============================================================================

#include "qx_engine_internal.h"

#include <new>

namespace {

static const char* const kSegmentIds[QX_SEGMENT_COUNT] = {
    QX_SEGMENT_CORE, QX_SEGMENT_UI,   QX_SEGMENT_DATA, QX_SEGMENT_IMG,
    QX_SEGMENT_NET,  QX_SEGMENT_AI,   QX_SEGMENT_SEC,  QX_SEGMENT_LOG,
    QX_SEGMENT_TEMP
};

static QXResult build_segment_configs(
    QXManifestHandle manifest,
    QXSegmentConfig  out[QX_SEGMENT_COUNT]) noexcept
{
    double soft_mb = 0.0;
    double hard_mb = 0.0;
    QXResult rc = qx_manifest_soft_limit_mb(manifest, &soft_mb);
    if (rc != QX_OK) return rc;
    rc = qx_manifest_hard_limit_mb(manifest, &hard_mb);
    if (rc != QX_OK) return rc;

    for (uint32_t i = 0u; i < QX_SEGMENT_COUNT; ++i) {
        std::strncpy(out[i].segment_id, kSegmentIds[i],
                     sizeof(out[i].segment_id) - 1u);
        out[i].segment_id[sizeof(out[i].segment_id) - 1u] = '\0';
        out[i].effective_soft_bytes =
            static_cast<QXSize>(soft_mb * 1024.0 * 1024.0
                                / static_cast<double>(QX_SEGMENT_COUNT));
        out[i].effective_hard_bytes =
            static_cast<QXSize>(hard_mb * 1024.0 * 1024.0
                                / static_cast<double>(QX_SEGMENT_COUNT));
        out[i].max_slots = (i < 6u) ? QX_SLOTS_HIGH_PRIORITY
                                     : QX_SLOTS_LOW_PRIORITY;
    }
    return QX_OK;
}

static QXResult validate_manifest(QXManifestHandle manifest) noexcept {
    if (!manifest) return QX_ERR_NULL_ARG;
    QXBool laws_active = QX_FALSE;
    QXResult rc = qx_manifest_all_laws_active(manifest, &laws_active);
    if (rc != QX_OK || !laws_active) return QX_ERR_MANIFEST_INVALID;
    QXBool ethical = QX_FALSE;
    rc = qx_manifest_is_fully_ethical(manifest, &ethical);
    if (rc != QX_OK || !ethical) return QX_ERR_MANIFEST_INVALID;
    return QX_OK;
}

static void load_ethics_flags(QXManifestHandle manifest,
                               QXEngine_s*      e) noexcept
{
    QXManifestEthics eth{};
    if (qx_manifest_ethics(manifest, &eth) != QX_OK) return;
    e->ethics_dark_patterns    = eth.dark_patterns_prohibited;
    e->ethics_deceptive_flows  = eth.deceptive_flows_prohibited;
    e->ethics_manipulative_ux  = eth.manipulative_ux_prohibited;
    e->ethics_privacy_first    = eth.privacy_first_design;
    e->ethics_transparent_data = eth.transparent_data_usage;
}

static void load_law7_flags(QXManifestHandle manifest,
                             QXEngine_s*      e) noexcept
{
    QXManifestCreativity cr{};
    if (qx_manifest_creativity(manifest, &cr) != QX_OK) return;
    e->native_first_policy = cr.native_first_policy;

    const uint32_t cap_count = cr.native_capability_count;
    for (uint32_t i = 0u; i < cap_count && i < QX_EVAL_MAX_CAPABILITIES; ++i) {
        if (cr.native_capabilities[i].declared) {
            e->capabilities.add_declared(cr.native_capabilities[i].id);
        }
    }
}

} // namespace

extern "C" {

QXResult qx_engine_create(const QXEngineConfig* config,
                           QXEngineHandle*       out_engine)
{
    if (!config || !out_engine)      return QX_ERR_NULL_HANDLE;
    if (!config->manifest_json)      return QX_ERR_NULL_ARG;

    QXManifestHandle           manifest = nullptr;
    QXManifestValidationResult vresult{};
    QXResult rc = qx_manifest_parse(config->manifest_json,
                                     config->manifest_json_length,
                                     &manifest, &vresult);
    if (rc != QX_OK) return rc;

    rc = validate_manifest(manifest);
    if (rc != QX_OK) { qx_manifest_destroy(manifest); return rc; }

    auto* e = new (std::nothrow) QXEngine_s{};
    if (!e) { qx_manifest_destroy(manifest); return QX_ERR_INTERNAL; }
    e->manifest      = manifest;
    e->created_at_ms = qx_engine_now_ms();

    load_ethics_flags(manifest, e);
    load_law7_flags(manifest, e);

    QXSegmentConfig seg_cfgs[QX_SEGMENT_COUNT]{};
    rc = build_segment_configs(manifest, seg_cfgs);
    if (rc != QX_OK) goto fail_manifest;

    rc = qx_memloc_create(seg_cfgs, QX_SEGMENT_COUNT, &e->memloc);
    if (rc != QX_OK) goto fail_manifest;

    rc = qx_pressure_create(e->memloc, &e->pressure);
    if (rc != QX_OK) goto fail_memloc;

    {
        QXEnforcerConfig encfg{};
        encfg.min_knowledge_score        = QX_MIN_KNOWLEDGE_SCORE;
        encfg.min_native_utilisation     = QX_MIN_NATIVE_UTILISATION;
        encfg.max_battery_drain_pct      = QX_MAX_BATTERY_DRAIN_PCT;
        encfg.max_network_redundancy_pct = QX_MAX_NETWORK_REDUNDANCY;
        encfg.equilibrium_overload_pct   = QX_EQUILIBRIUM_OVERLOAD_PCT;
        encfg.equilibrium_starvation_pct = QX_EQUILIBRIUM_STARVE_PCT;
        encfg.min_pairs_ratio            = QX_MIN_PAIRS_RATIO;
        encfg.fail_fast_on_fatal         = QX_FALSE;
        encfg.measure_duration           = QX_TRUE;

        rc = qx_law_enforcer_create(&encfg, &e->enforcer);
        if (rc != QX_OK) goto fail_pressure;
    }

    rc = qx_snapshot_history_create(&e->snapshots);
    if (rc != QX_OK) goto fail_enforcer;

    {
        QXTelemetryConfig tcfg{};
        tcfg.enabled                = QX_TRUE;
        tcfg.record_allocations     = QX_TRUE;
        tcfg.record_evictions       = QX_TRUE;
        tcfg.record_evaluations     = QX_TRUE;
        tcfg.record_snapshots       = QX_TRUE;
        tcfg.record_security        = QX_TRUE;
        tcfg.separate_violation_log = QX_TRUE;
        tcfg.verbose                = config->verbose;

        rc = qx_telemetry_create(&tcfg, &e->telemetry);
        if (rc != QX_OK) goto fail_snapshots;
    }

    rc = qx_bridge_create(&e->memloc_bridge, manifest, config->device_ram_bytes);
    if (rc != QX_OK) goto fail_telemetry;

    e->state = QX_ENGINE_STATE_CONFIGURED;
    *out_engine = e;
    return QX_OK;

fail_telemetry:  qx_telemetry_destroy(e->telemetry);
fail_snapshots:  qx_snapshot_history_destroy(e->snapshots);
fail_enforcer:   qx_law_enforcer_destroy(e->enforcer);
fail_pressure:   qx_pressure_destroy(e->pressure);
fail_memloc:     qx_memloc_destroy(e->memloc);
fail_manifest:   qx_manifest_destroy(manifest);
    delete e;
    return rc;
}

void qx_engine_destroy(QXEngineHandle engine)
{
    if (!engine) return;
    qx_telemetry_destroy(engine->telemetry);
    qx_snapshot_history_destroy(engine->snapshots);
    qx_law_enforcer_destroy(engine->enforcer);
    qx_pressure_destroy(engine->pressure);
    qx_memloc_destroy(engine->memloc);
    qx_bridge_destroy(&engine->memloc_bridge);
    qx_manifest_destroy(engine->manifest);
    delete engine;
}

QXResult qx_engine_start(QXEngineHandle engine)
{
    if (!engine) return QX_ERR_NULL_HANDLE;
    std::unique_lock lk(engine->state_mtx);
    if (engine->state == QX_ENGINE_STATE_RUNNING)  return QX_ERR_ALREADY_RUNNING;
    if (engine->state == QX_ENGINE_STATE_STOPPED)  return QX_ERR_ALREADY_STOPPED;
    if (engine->state != QX_ENGINE_STATE_CONFIGURED &&
        engine->state != QX_ENGINE_STATE_PAUSED)   return QX_ERR_NOT_CONFIGURED;
    engine->state = QX_ENGINE_STATE_RUNNING;
    qx_bridge_notify_lifecycle(&engine->memloc_bridge, 0u);
    return QX_OK;
}

QXResult qx_engine_pause(QXEngineHandle engine)
{
    if (!engine) return QX_ERR_NULL_HANDLE;
    std::unique_lock lk(engine->state_mtx);
    if (engine->state != QX_ENGINE_STATE_RUNNING) return QX_ERR_NOT_READY;
    engine->state = QX_ENGINE_STATE_PAUSED;
    qx_bridge_notify_lifecycle(&engine->memloc_bridge, 1u);
    return QX_OK;
}

QXResult qx_engine_resume(QXEngineHandle engine)
{
    if (!engine) return QX_ERR_NULL_HANDLE;
    std::unique_lock lk(engine->state_mtx);
    if (engine->state != QX_ENGINE_STATE_PAUSED) return QX_ERR_NOT_READY;
    engine->state = QX_ENGINE_STATE_RUNNING;
    qx_bridge_notify_lifecycle(&engine->memloc_bridge, 0u);
    return QX_OK;
}

QXResult qx_engine_stop(QXEngineHandle engine)
{
    if (!engine) return QX_ERR_NULL_HANDLE;
    std::unique_lock lk(engine->state_mtx);
    if (engine->state == QX_ENGINE_STATE_STOPPED) return QX_ERR_ALREADY_STOPPED;
    engine->state = QX_ENGINE_STATE_STOPPED;
    qx_bridge_notify_lifecycle(&engine->memloc_bridge, 1u);
    return QX_OK;
}

QXResult qx_engine_state(QXEngineHandle engine, QXEngineState* out_state)
{
    if (!engine || !out_state) return QX_ERR_NULL_HANDLE;
    std::shared_lock lk(engine->state_mtx);
    *out_state = engine->state;
    return QX_OK;
}

QXResult qx_engine_allocate(QXEngineHandle  engine,
                              const char*     label,
                              const char*     segment_id,
                              QXLeafClassId   leaf_class,
                              QXSize          size_bytes,
                              QXLeafHandle*   out_leaf)
{
    if (!engine || !out_leaf) return QX_ERR_NULL_HANDLE;

    {
        std::shared_lock lk(engine->state_mtx);
        if (engine->state != QX_ENGINE_STATE_RUNNING) return QX_ERR_NOT_READY;
    }

    if (!label || label[0] == '\0') {
        engine->unlabelled_alloc_count.fetch_add(1u,
                                                  std::memory_order_relaxed);
        return QX_ERR_LABEL_BLANK;
    }
    const size_t label_len = std::strlen(label);
    if (label_len < QX_LABEL_MIN_LENGTH) {
        engine->unlabelled_alloc_count.fetch_add(1u,
                                                  std::memory_order_relaxed);
        return QX_ERR_LABEL_TOO_SHORT;
    }
    if (label_len > QX_LABEL_MAX_LENGTH) {
        engine->unlabelled_alloc_count.fetch_add(1u,
                                                  std::memory_order_relaxed);
        return QX_ERR_LABEL_TOO_LONG;
    }

    QXAllocationRequest request{};
    request.label = label;
    request.segment_id = segment_id;
    request.slot_id = "engine";
    request.size_bytes = size_bytes;
    request.leaf_class = leaf_class;
    request.purpose = "qx_engine_allocate";

    QXConstitutionalAllocation constitutional_alloc{};
    QXResult rc = qx_bridge_allocate(
        &engine->memloc_bridge, &request, &constitutional_alloc);
    if (rc != QX_OK) return rc;

    QXAllocResult ar{};
    rc = qx_memloc_allocate(engine->memloc, label,
                            segment_id, leaf_class,
                            size_bytes, &ar);
    if (rc != QX_OK) {
        qx_memloc_constitutional_deallocate(
            &engine->memloc_bridge.authority,
            constitutional_alloc.leaf_handle);
        return rc;
    }

    char legacy_id[37]{};
    rc = qx_leaf_id(ar.leaf, legacy_id);
    if (rc != QX_OK) {
        QXSize ignored = 0u;
        qx_memloc_deallocate(engine->memloc, legacy_id, &ignored);
        qx_memloc_constitutional_deallocate(
            &engine->memloc_bridge.authority,
            constitutional_alloc.leaf_handle);
        return rc;
    }

    rc = qx_bridge_bind_legacy_leaf(
        &engine->memloc_bridge,
        legacy_id,
        constitutional_alloc.leaf_handle);
    if (rc != QX_OK) {
        QXSize ignored = 0u;
        qx_memloc_deallocate(engine->memloc, legacy_id, &ignored);
        qx_memloc_constitutional_deallocate(
            &engine->memloc_bridge.authority,
            constitutional_alloc.leaf_handle);
        return rc;
    }

    if (rc == QX_OK) {
        *out_leaf = ar.leaf;
        engine->alloc_count.fetch_add(1u, std::memory_order_relaxed);

        QXTelemetryEventInput ev{};
        ev.category = QX_TEL_ALLOCATION;
        ev.result   = QX_OK;
        ev.bytes    = size_bytes;
        if (segment_id)
            std::strncpy(ev.segment_id, segment_id,
                         sizeof(ev.segment_id) - 1u);
        if (label)
            std::strncpy(ev.leaf_label, label,
                         sizeof(ev.leaf_label) - 1u);
        qx_telemetry_record(engine->telemetry, &ev, nullptr);
    }
    return rc;
}

QXResult qx_engine_deallocate(QXEngineHandle engine, const char* leaf_id)
{
    if (!engine || !leaf_id) return QX_ERR_NULL_HANDLE;
    {
        std::shared_lock lk(engine->state_mtx);
        if (engine->state != QX_ENGINE_STATE_RUNNING) return QX_ERR_NOT_READY;
    }

    QXSize freed = 0u;
    QXResult rc = qx_memloc_deallocate(engine->memloc, leaf_id, &freed);
    if (rc == QX_OK) {
        qx_bridge_deallocate_legacy(&engine->memloc_bridge, leaf_id);
        engine->free_count.fetch_add(1u, std::memory_order_relaxed);

        QXTelemetryEventInput ev{};
        ev.category = QX_TEL_DEALLOCATION;
        ev.result   = QX_OK;
        ev.bytes    = freed;
        qx_telemetry_record(engine->telemetry, &ev, nullptr);
    }
    return rc;
}

QXResult qx_engine_touch(QXEngineHandle engine, const char* leaf_id)
{
    if (!engine || !leaf_id) return QX_ERR_NULL_HANDLE;
    {
        std::shared_lock lk(engine->state_mtx);
        if (engine->state != QX_ENGINE_STATE_RUNNING) return QX_ERR_NOT_READY;
    }
    return qx_memloc_touch(engine->memloc, leaf_id);
}

} // extern "C"

static_assert(QX_SEGMENT_COUNT == 9u,  "segment count mismatch");
static_assert(QX_LAW_COUNT     == 8u,  "law count mismatch");
static_assert(QX_SLOT_COUNT    == 60u, "slot count mismatch");
