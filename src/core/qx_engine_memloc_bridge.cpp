// =============================================================================
// qx_engine_memloc_bridge.cpp
// QXEngine Core – Constitutional Memory Bridge Implementation
// =============================================================================
// Owner : Masa Bayu
// Created: 2026-05-25
// Repo   : https://github.com/qxengine/qxengine-core
// Path   : src/core/qx_engine_memloc_bridge.cpp
// License: Apache 2.0
//

#include "qx_engine_memloc_bridge.h"

#include "qxengine/law/qx_law_enforcer.h"

#include <algorithm>
#include <cstring>

namespace {

double bridge_device_scale(QXSize device_ram_bytes) noexcept {
    constexpr QXSize kGiB = 1024ULL * 1024ULL * 1024ULL;
    if (device_ram_bytes >= 6ULL * kGiB) return 2.0;
    if (device_ram_bytes > 0u && device_ram_bytes < 2ULL * kGiB) return 0.5;
    return 1.0;
}

QXResult bridge_build_config(QXManifestHandle manifest,
                             QXSize device_ram_bytes,
                             QXConstitutionalConfig* out) noexcept {
    if (!manifest || !out) return QX_ERR_NULL_HANDLE;

    double soft_mb = 0.0;
    double hard_mb = 0.0;
    QXResult rc = qx_manifest_soft_limit_mb(manifest, &soft_mb);
    if (rc != QX_OK) return rc;
    rc = qx_manifest_hard_limit_mb(manifest, &hard_mb);
    if (rc != QX_OK) return rc;

    static char app_id[QX_MANIFEST_APP_ID_MAX];
    std::memset(app_id, 0, sizeof(app_id));
    qx_manifest_app_id(manifest, app_id);

    std::memset(out, 0, sizeof(*out));
    out->soft_budget_bytes = static_cast<QXSize>(soft_mb * 1024.0 * 1024.0);
    out->hard_budget_bytes = static_cast<QXSize>(hard_mb * 1024.0 * 1024.0);
    out->device_scale = bridge_device_scale(device_ram_bytes);
    out->declared_x = 1.0;
    out->tolerance_pct = 15.0;
    out->instance_label = app_id;
    return QX_OK;
}

void bridge_recompute_report(QXLawReport* report) noexcept {
    double health = 0.0;
    QXBool fully_compliant = QX_TRUE;
    QXBool has_critical = QX_FALSE;
    QXBool has_fatal = QX_FALSE;

    for (uint32_t i = 0u; i < QX_LAW_COUNT; ++i) {
        QXLawScore* score = &report->law_scores[i];
        score->weighted_score = score->raw_score * QX_LAW_WEIGHTS[i];
        health += score->weighted_score;
        if (score->raw_score < QX_SCORE_SOVEREIGN ||
            score->violation_count > 0u) {
            fully_compliant = QX_FALSE;
        }
        if (score->worst_severity >= QX_SEVERITY_CRITICAL) {
            has_critical = QX_TRUE;
        }
        if (score->worst_severity >= QX_SEVERITY_FATAL) {
            has_fatal = QX_TRUE;
        }
    }

    report->health_score = std::clamp(health, 0.0, 100.0);
    report->certification = qx_certification_from_score(report->health_score);
    report->is_fully_compliant = fully_compliant;
    report->has_critical_violation = has_critical;
    report->has_fatal_violation = has_fatal;
}

void bridge_merge_report(const QXConstitutionalReport& source,
                         QXLawReport* target) noexcept {
    for (uint32_t i = 0u; i < QX_LAW_COUNT; ++i) {
        const double raw = source.law_report.law_scores[i].raw_score;
        if (raw > 0.0 && raw < target->law_scores[i].raw_score) {
            target->law_scores[i].raw_score = raw;
        }
    }
    bridge_recompute_report(target);
}

QXEngineMemlocBinding* bridge_find_binding(QXEngineMemlocBridge* bridge,
                                           const char* legacy_id) noexcept {
    if (!bridge || !legacy_id) return nullptr;
    for (auto& binding : bridge->bindings) {
        if (binding.active &&
            std::strncmp(binding.legacy_leaf_id, legacy_id,
                         QX_ENGINE_LEAF_ID_BYTES) == 0) {
            return &binding;
        }
    }
    return nullptr;
}

} // namespace

QXResult qx_bridge_create(QXEngineMemlocBridge* bridge,
                          QXManifestHandle manifest,
                          QXSize device_ram_bytes) {
    if (!bridge || !manifest) return QX_ERR_NULL_HANDLE;

    std::memset(bridge, 0, sizeof(*bridge));
    QXConstitutionalConfig config{};
    QXResult rc = bridge_build_config(manifest, device_ram_bytes, &config);
    if (rc != QX_OK) return rc;

    rc = qx_memloc_constitutional_create_config(&config, &bridge->authority);
    if (rc != QX_OK) {
        std::memset(bridge, 0, sizeof(*bridge));
        return rc;
    }

    bridge->initialized = QX_TRUE;
    bridge->last_pressure_tier = 0u;
    return QX_OK;
}

void qx_bridge_destroy(QXEngineMemlocBridge* bridge) {
    if (!bridge || bridge->initialized != QX_TRUE) return;
    qx_memloc_constitutional_destroy(&bridge->authority);
    std::memset(bridge, 0, sizeof(*bridge));
}

QXResult qx_bridge_run_cycle(QXEngineMemlocBridge* bridge,
                             QXTimestamp now_ms,
                             QXLawReport* law_report) {
    if (!bridge || bridge->initialized != QX_TRUE || !law_report) {
        return QX_ERR_NULL_HANDLE;
    }

    QXConstitutionalReport report{};
    QXResult rc = qx_memloc_constitutional_report(
        &bridge->authority, now_ms, &report);
    if (rc != QX_OK) return rc;

    bridge->last_report = report;
    bridge_merge_report(report, law_report);
    return QX_OK;
}

QXResult qx_bridge_notify_pressure(QXEngineMemlocBridge* bridge,
                                   uint8_t pressure_tier) {
    if (!bridge || bridge->initialized != QX_TRUE) return QX_ERR_NULL_HANDLE;
    if (pressure_tier < QX_TIER_NORMAL || pressure_tier > QX_TIER_CRITICAL) {
        return QX_ERR_INVALID_ARGUMENT;
    }
    if (pressure_tier == bridge->last_pressure_tier) return QX_OK;

    QXResult rc = qx_memloc_constitutional_notify_pressure(
        &bridge->authority, pressure_tier);
    if (rc == QX_OK) bridge->last_pressure_tier = pressure_tier;
    return rc;
}

QXResult qx_bridge_notify_lifecycle(QXEngineMemlocBridge* bridge,
                                    uint8_t lifecycle_state) {
    if (!bridge || bridge->initialized != QX_TRUE) return QX_ERR_NULL_HANDLE;
    return qx_memloc_constitutional_notify_lifecycle(
        &bridge->authority, lifecycle_state);
}

QXResult qx_bridge_notify_screen(QXEngineMemlocBridge* bridge,
                                 const char* screen_profile_id) {
    if (!bridge || bridge->initialized != QX_TRUE || !screen_profile_id) {
        return QX_ERR_NULL_HANDLE;
    }
    return qx_memloc_constitutional_notify_screen(
        &bridge->authority, screen_profile_id);
}

QXResult qx_bridge_allocate(QXEngineMemlocBridge* bridge,
                            const QXAllocationRequest* request,
                            QXConstitutionalAllocation* out_allocation) {
    if (!bridge || bridge->initialized != QX_TRUE) return QX_ERR_NULL_HANDLE;
    QXResult rc = qx_memloc_constitutional_allocate(
        &bridge->authority, request, out_allocation);
    if (rc == QX_OK) ++bridge->total_allocations;
    return rc;
}

QXResult qx_bridge_bind_legacy_leaf(QXEngineMemlocBridge* bridge,
                                    const char* legacy_leaf_id,
                                    QXLeafHandle constitutional_leaf) {
    if (!bridge || !legacy_leaf_id || !constitutional_leaf) {
        return QX_ERR_NULL_HANDLE;
    }

    for (auto& binding : bridge->bindings) {
        if (binding.active == QX_FALSE) {
            std::strncpy(binding.legacy_leaf_id, legacy_leaf_id,
                         sizeof(binding.legacy_leaf_id) - 1u);
            binding.constitutional_leaf = constitutional_leaf;
            binding.active = QX_TRUE;
            return QX_OK;
        }
    }
    return QX_ERR_NO_SLOT;
}

QXResult qx_bridge_deallocate_legacy(QXEngineMemlocBridge* bridge,
                                     const char* legacy_leaf_id) {
    QXEngineMemlocBinding* binding =
        bridge_find_binding(bridge, legacy_leaf_id);
    if (!binding) return QX_ERR_LEAF_NOT_FOUND;

    QXResult rc = qx_memloc_constitutional_deallocate(
        &bridge->authority, binding->constitutional_leaf);
    binding->active = QX_FALSE;
    binding->constitutional_leaf = nullptr;
    ++bridge->total_deallocations;
    return rc;
}

double qx_bridge_health_score(const QXEngineMemlocBridge* bridge) {
    if (!bridge || bridge->initialized != QX_TRUE) return 0.0;
    return qx_memloc_constitutional_health_score(&bridge->authority);
}

QXCertificationTier qx_bridge_certification(
    const QXEngineMemlocBridge* bridge) {
    if (!bridge || bridge->initialized != QX_TRUE) {
        return QX_CERTIFICATION_NONE;
    }
    return qx_memloc_constitutional_certification(&bridge->authority);
}
