// =============================================================================
// QXEngine Core – src/manifest/qx_manifest_validation.cpp
// Owner : Masa Bayu
// Created: 2026-05-25
// Repo   : https://github.com/qxengine/qxengine-core
// Purpose: QXManifest validation rules MFT-0001 through MFT-0014.
// =============================================================================

#include "qx_manifest_internal.h"

#include <cstring>

namespace {

static bool str_blank(const char *s) {
    if (!s || s[0] == '\0') return true;
    while (*s) { if (static_cast<unsigned char>(*s) > 0x20u) return false; ++s; }
    return true;
}

static void add_error(QXManifestValidationResult *r,
                      const char *code, const char *msg, const char *field) {
    if (!r || r->error_count >= QX_MANIFEST_MAX_ERRORS) return;
    QXManifestValidationError *e = &r->errors[r->error_count++];
    std::strncpy(e->code,       code,  sizeof(e->code)       - 1u); e->code[sizeof(e->code)-1u]             = '\0';
    std::strncpy(e->message,    msg,   sizeof(e->message)    - 1u); e->message[sizeof(e->message)-1u]       = '\0';
    std::strncpy(e->field_path, field, sizeof(e->field_path) - 1u); e->field_path[sizeof(e->field_path)-1u] = '\0';
}

static void rule_0001(const QXManifest_s *m, QXManifestValidationResult *r) {
    if (std::strncmp(m->meta.spec_version, "1.0.0", 5) != 0)
        add_error(r, "MFT-0001", "spec_version must be 1.0.0", "meta.spec_version");
}
static void rule_0002(const QXManifest_s *m, QXManifestValidationResult *r) {
    if (str_blank(m->identity.app_id))
        add_error(r, "MFT-0002", "app_id must not be blank", "identity.app_id");
    else if (std::strlen(m->identity.app_id) > QX_MANIFEST_APP_ID_MAX - 1u)
        add_error(r, "MFT-0002", "app_id exceeds 128 characters", "identity.app_id");
}
static void rule_0003(const QXManifest_s *m, QXManifestValidationResult *r) {
    if (str_blank(m->identity.app_name))
        add_error(r, "MFT-0003", "app_name must not be blank", "identity.app_name");
}
static void rule_0004(const QXManifest_s *m, QXManifestValidationResult *r) {
    if (str_blank(m->identity.owner))
        add_error(r, "MFT-0004", "owner must not be blank", "identity.owner");
}
static void rule_0005(const QXManifest_s *m, QXManifestValidationResult *r) {
    const char *p = m->identity.platform;
    bool ok = (std::strcmp(p,"android")==0 || std::strcmp(p,"ios")==0 ||
               std::strcmp(p,"web")==0     || std::strcmp(p,"desktop")==0);
    if (!ok)
        add_error(r, "MFT-0005", "platform must be android, ios, web, or desktop",
                  "identity.platform");
}
static void rule_0006(const QXManifest_s *m, QXManifestValidationResult *r) {
    if (m->limit.memory_soft_limit_mb < static_cast<double>(QX_MIN_SOFT_LIMIT_MB))
        add_error(r, "MFT-0006", "soft_limit_mb must be >= 64", "limit.memory_soft_limit_mb");
}
static void rule_0007(const QXManifest_s *m, QXManifestValidationResult *r) {
    if (m->limit.memory_hard_limit_mb < static_cast<double>(QX_MIN_HARD_LIMIT_MB))
        add_error(r, "MFT-0007", "hard_limit_mb must be >= 128", "limit.memory_hard_limit_mb");
    if (m->limit.memory_hard_limit_mb <= m->limit.memory_soft_limit_mb)
        add_error(r, "MFT-0007", "hard_limit_mb must exceed soft_limit_mb",
                  "limit.memory_hard_limit_mb");
}
static void rule_0008(const QXManifest_s *m, QXManifestValidationResult *r) {
    if (m->limit.segment_count != QX_SEGMENT_COUNT)
        add_error(r, "MFT-0008", "limit must declare exactly 9 segments", "limit.segments");
}
static void rule_0009(const QXManifest_s *m, QXManifestValidationResult *r) {
    double total = 0.0;
    for (uint32_t i = 0u; i < m->limit.segment_count && i < QX_SEGMENT_COUNT; ++i)
        total += m->limit.segments[i].budget_percent;
    if (static_cast<uint32_t>(total + 0.5) != 100u)
        add_error(r, "MFT-0009", "segment budget_percent values must sum to 100",
                  "limit.segments[*].budget_percent");
}
static void rule_0010(const QXManifest_s *m, QXManifestValidationResult *r) {
    if (!m->law_compliance.all_laws_active)
        add_error(r, "MFT-0010", "all eight constitutional laws must be active",
                  "law_compliance.all_laws_active");
}
static void rule_0011(const QXManifest_s *m, QXManifestValidationResult *r) {
    if (!m->ethics.is_fully_ethical)
        add_error(r, "MFT-0011", "all five ethics flags must be true",
                  "ethics.is_fully_ethical");
}
static void rule_0012(const QXManifest_s *m, QXManifestValidationResult *r) {
    if (!m->creativity.native_first_policy)
        add_error(r, "MFT-0012", "creativity.native_first_policy must be true",
                  "creativity.native_first_policy");
}
static void rule_0013(const QXManifest_s *m, QXManifestValidationResult *r) {
    if (m->economy.battery_drain_max_pct_per_10min > QX_MAX_BATTERY_DRAIN_PCT)
        add_error(r, "MFT-0013", "battery_drain_max_pct_per_10min must be <= 10.0",
                  "economy.battery_drain_max_pct_per_10min");
}
static void rule_0014(const QXManifest_s *m, QXManifestValidationResult *r) {
    if (m->knowledge.min_knowledge_score < 90.0)
        add_error(r, "MFT-0014", "knowledge.min_knowledge_score must be >= 90.0",
                  "knowledge.min_knowledge_score");
}

} // namespace

void qx_manifest_run_all_rules(const QXManifest_s *m,
                               QXManifestValidationResult *r) {
    rule_0001(m, r); rule_0002(m, r); rule_0003(m, r); rule_0004(m, r);
    rule_0005(m, r); rule_0006(m, r); rule_0007(m, r); rule_0008(m, r);
    rule_0009(m, r); rule_0010(m, r); rule_0011(m, r); rule_0012(m, r);
    rule_0013(m, r); rule_0014(m, r);
    r->is_valid = (r->error_count == 0) ? QX_TRUE : QX_FALSE;
}
