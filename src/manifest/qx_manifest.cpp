// =============================================================================
// QXEngine Core – src/manifest/qx_manifest.cpp
// Owner : Masa Bayu
// Created: 2026-05-24
// Repo   : https://github.com/qxengine/qxengine-core
// Purpose: QXManifest public lifecycle, accessors, and serialisation.
// =============================================================================

#include "qx_manifest_internal.h"

#include <cstdio>
#include <cstring>
#include <new>

extern "C" {

QX_API QXResult qx_manifest_parse(
    const char                  *json_utf8,
    uint32_t                     json_length,
    QXManifestHandle            *out_manifest,
    QXManifestValidationResult  *out_result)
{
    if (!json_utf8 || !out_manifest || !out_result) return QX_ERR_NULL_HANDLE;
    auto *m = new (std::nothrow) QXManifest_s{};
    if (!m) return QX_ERR_INTERNAL;
    if (!qx_manifest_parse_json(json_utf8, json_length, m)) {
        delete m;
        return QX_ERR_MANIFEST_PARSE;
    }
    QXManifestValidationResult local_result{};
    qx_manifest_run_all_rules(m, &local_result);
    *out_result = local_result;
    if (!local_result.is_valid) {
        delete m;
        *out_manifest = nullptr;
        return QX_ERR_MANIFEST_INVALID;
    }
    *out_manifest = m;
    return QX_OK;
}

QX_API QXResult qx_manifest_validate(
    QXManifestHandle             manifest,
    QXManifestValidationResult  *out_result)
{
    if (!manifest || !out_result) return QX_ERR_NULL_HANDLE;
    std::lock_guard<std::mutex> lk(manifest->mtx);
    out_result->error_count = 0u;
    qx_manifest_run_all_rules(manifest, out_result);
    return (out_result->is_valid) ? QX_OK : QX_ERR_MANIFEST_INVALID;
}

QX_API void qx_manifest_destroy(QXManifestHandle manifest) { delete manifest; }

QX_API QXResult qx_manifest_identity(QXManifestHandle m, QXManifestIdentity *out) {
    if (!m || !out) return QX_ERR_NULL_HANDLE;
    std::lock_guard<std::mutex> lk(m->mtx); *out = m->identity; return QX_OK;
}
QX_API QXResult qx_manifest_law_compliance(QXManifestHandle m, QXManifestLawCompliance *out) {
    if (!m || !out) return QX_ERR_NULL_HANDLE;
    std::lock_guard<std::mutex> lk(m->mtx); *out = m->law_compliance; return QX_OK;
}
QX_API QXResult qx_manifest_limit(QXManifestHandle m, QXManifestLimit *out) {
    if (!m || !out) return QX_ERR_NULL_HANDLE;
    std::lock_guard<std::mutex> lk(m->mtx); *out = m->limit; return QX_OK;
}
QX_API QXResult qx_manifest_segment(
    QXManifestHandle m, const char *segment_id, QXManifestSegment *out)
{
    if (!m || !segment_id || !out) return QX_ERR_NULL_HANDLE;
    std::lock_guard<std::mutex> lk(m->mtx);
    for (uint32_t i = 0u; i < m->limit.segment_count; ++i) {
        if (std::strncmp(m->limit.segments[i].id, segment_id,
                         QX_MANIFEST_SEGMENT_ID_MAX - 1u) == 0) {
            *out = m->limit.segments[i]; return QX_OK;
        }
    }
    return QX_ERR_UNKNOWN_SEGMENT;
}
QX_API QXResult qx_manifest_ethics(QXManifestHandle m, QXManifestEthics *out) {
    if (!m || !out) return QX_ERR_NULL_HANDLE;
    std::lock_guard<std::mutex> lk(m->mtx); *out = m->ethics; return QX_OK;
}
QX_API QXResult qx_manifest_creativity(QXManifestHandle m, QXManifestCreativity *out) {
    if (!m || !out) return QX_ERR_NULL_HANDLE;
    std::lock_guard<std::mutex> lk(m->mtx); *out = m->creativity; return QX_OK;
}
QX_API QXResult qx_manifest_knowledge(QXManifestHandle m, QXManifestKnowledge *out) {
    if (!m || !out) return QX_ERR_NULL_HANDLE;
    std::lock_guard<std::mutex> lk(m->mtx); *out = m->knowledge; return QX_OK;
}
QX_API QXResult qx_manifest_economy(QXManifestHandle m, QXManifestEconomy *out) {
    if (!m || !out) return QX_ERR_NULL_HANDLE;
    std::lock_guard<std::mutex> lk(m->mtx); *out = m->economy; return QX_OK;
}
QX_API QXResult qx_manifest_verticals(QXManifestHandle m, QXManifestVerticals *out) {
    if (!m || !out) return QX_ERR_NULL_HANDLE;
    std::lock_guard<std::mutex> lk(m->mtx); *out = m->verticals; return QX_OK;
}
QX_API QXResult qx_manifest_meta(QXManifestHandle m, QXManifestMeta *out) {
    if (!m || !out) return QX_ERR_NULL_HANDLE;
    std::lock_guard<std::mutex> lk(m->mtx); *out = m->meta; return QX_OK;
}
QX_API QXResult qx_manifest_app_id(QXManifestHandle m, char *buffer) {
    if (!m || !buffer) return QX_ERR_NULL_HANDLE;
    std::lock_guard<std::mutex> lk(m->mtx);
    std::strncpy(buffer, m->identity.app_id, QX_MANIFEST_APP_ID_MAX - 1u);
    buffer[QX_MANIFEST_APP_ID_MAX - 1u] = '\0'; return QX_OK;
}
QX_API QXResult qx_manifest_platform(QXManifestHandle m, char *buffer) {
    if (!m || !buffer) return QX_ERR_NULL_HANDLE;
    std::lock_guard<std::mutex> lk(m->mtx);
    std::strncpy(buffer, m->identity.platform, QX_MANIFEST_PLATFORM_MAX - 1u);
    buffer[QX_MANIFEST_PLATFORM_MAX - 1u] = '\0'; return QX_OK;
}
QX_API QXResult qx_manifest_soft_limit_mb(QXManifestHandle m, double *out_mb) {
    if (!m || !out_mb) return QX_ERR_NULL_HANDLE;
    std::lock_guard<std::mutex> lk(m->mtx);
    *out_mb = m->limit.memory_soft_limit_mb; return QX_OK;
}
QX_API QXResult qx_manifest_hard_limit_mb(QXManifestHandle m, double *out_mb) {
    if (!m || !out_mb) return QX_ERR_NULL_HANDLE;
    std::lock_guard<std::mutex> lk(m->mtx);
    *out_mb = m->limit.memory_hard_limit_mb; return QX_OK;
}
QX_API QXResult qx_manifest_all_laws_active(QXManifestHandle m, QXBool *out) {
    if (!m || !out) return QX_ERR_NULL_HANDLE;
    std::lock_guard<std::mutex> lk(m->mtx);
    *out = m->law_compliance.all_laws_active; return QX_OK;
}
QX_API QXResult qx_manifest_is_fully_ethical(QXManifestHandle m, QXBool *out) {
    if (!m || !out) return QX_ERR_NULL_HANDLE;
    std::lock_guard<std::mutex> lk(m->mtx);
    *out = m->ethics.is_fully_ethical; return QX_OK;
}
QX_API QXResult qx_manifest_json_size(QXManifestHandle m, uint32_t *out_size_bytes) {
    if (!m || !out_size_bytes) return QX_ERR_NULL_HANDLE;
    *out_size_bytes = 4096u; return QX_OK;
}

QX_API QXResult qx_manifest_to_json(
    QXManifestHandle m, char *buffer, uint32_t buffer_size, uint32_t *out_written)
{
    if (!m || !buffer || buffer_size == 0u) return QX_ERR_NULL_HANDLE;
    std::lock_guard<std::mutex> lk(m->mtx);
    char segs[2048] = {};
    int  soff = 0;
    soff += std::snprintf(segs + soff, sizeof(segs) - static_cast<size_t>(soff), "[");
    for (uint32_t i = 0u; i < m->limit.segment_count; ++i) {
        const QXManifestSegment &s = m->limit.segments[i];
        soff += std::snprintf(segs + soff, sizeof(segs) - static_cast<size_t>(soff),
            "%s{\"id\":\"%s\",\"name\":\"%s\",\"budget_percent\":%.1f,"
            "\"soft_limit_mb\":%.2f,\"hard_limit_mb\":%.2f}",
            i > 0 ? "," : "",
            s.id, s.name, s.budget_percent, s.soft_limit_mb, s.hard_limit_mb);
    }
    soff += std::snprintf(segs + soff, sizeof(segs) - static_cast<size_t>(soff), "]");

    int n = std::snprintf(buffer, static_cast<size_t>(buffer_size),
        "{\"meta\":{\"spec_version\":\"%s\",\"generated_by\":\"%s\",\"validated_at\":\"%s\"},"
        "\"identity\":{\"app_id\":\"%s\",\"app_name\":\"%s\",\"owner\":\"%s\","
        "\"version\":\"%s\",\"created\":\"%s\",\"platform\":\"%s\","
        "\"minimum_engine_version\":\"%s\"},"
        "\"law_compliance\":{\"law1_pattern\":%s,\"law2_limit\":%s,\"law3_pairs\":%s,"
        "\"law4_equilibrium\":%s,\"law5_knowledge\":%s,\"law6_ethics\":%s,"
        "\"law7_creativity\":%s,\"law8_economy\":%s},"
        "\"limit\":{\"memory_soft_limit_mb\":%.1f,\"memory_hard_limit_mb\":%.1f,"
        "\"segments\":%s},"
        "\"ethics\":{\"dark_patterns_prohibited\":%s,\"deceptive_flows_prohibited\":%s,"
        "\"manipulative_ux_prohibited\":%s,\"privacy_first_design\":%s,"
        "\"transparent_data_usage\":%s},"
        "\"creativity\":{\"native_first_policy\":%s,\"native_utilisation_target\":%.2f},"
        "\"knowledge\":{\"snapshot_interval_seconds\":%.1f,\"min_knowledge_score\":%.1f,"
        "\"telemetry_enabled\":%s},"
        "\"economy\":{\"network_budget_mb_per_session\":%.1f,"
        "\"battery_drain_max_pct_per_10min\":%.1f,\"commerce_model\":\"%s\","
        "\"network_redundancy_max_pct\":%.1f},"
        "\"verticals\":{\"primary\":\"%s\"}}",
        m->meta.spec_version, m->meta.generated_by, m->meta.validated_at,
        m->identity.app_id, m->identity.app_name, m->identity.owner,
        m->identity.version, m->identity.created, m->identity.platform,
        m->identity.minimum_engine_version,
        m->law_compliance.law1_pattern     ? "true" : "false",
        m->law_compliance.law2_limit       ? "true" : "false",
        m->law_compliance.law3_pairs       ? "true" : "false",
        m->law_compliance.law4_equilibrium ? "true" : "false",
        m->law_compliance.law5_knowledge   ? "true" : "false",
        m->law_compliance.law6_ethics      ? "true" : "false",
        m->law_compliance.law7_creativity  ? "true" : "false",
        m->law_compliance.law8_economy     ? "true" : "false",
        m->limit.memory_soft_limit_mb, m->limit.memory_hard_limit_mb, segs,
        m->ethics.dark_patterns_prohibited   ? "true" : "false",
        m->ethics.deceptive_flows_prohibited ? "true" : "false",
        m->ethics.manipulative_ux_prohibited ? "true" : "false",
        m->ethics.privacy_first_design       ? "true" : "false",
        m->ethics.transparent_data_usage     ? "true" : "false",
        m->creativity.native_first_policy    ? "true" : "false",
        m->creativity.native_utilisation_target,
        m->knowledge.snapshot_interval_seconds,
        m->knowledge.min_knowledge_score,
        m->knowledge.telemetry_enabled ? "true" : "false",
        m->economy.network_budget_mb_per_session,
        m->economy.battery_drain_max_pct_per_10min,
        m->economy.commerce_model,
        m->economy.network_redundancy_max_pct,
        m->verticals.primary);
    if (n < 0) return QX_ERR_INTERNAL;
    if (out_written) *out_written = static_cast<uint32_t>(n);
    return (static_cast<uint32_t>(n) >= buffer_size) ? QX_ERR_BUFFER_TOO_SMALL : QX_OK;
}

} // extern "C"

static_assert(QX_SEGMENT_COUNT       == 9u,  "segment count mismatch");
static_assert(QX_MANIFEST_MAX_ERRORS == 32u, "error cap mismatch");
static_assert(QX_MIN_SOFT_LIMIT_MB   == 64u, "soft limit constant mismatch");
static_assert(QX_MIN_HARD_LIMIT_MB   == 128u,"hard limit constant mismatch");
