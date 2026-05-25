// =============================================================================
// QXEngine Core – src/manifest/qx_manifest_parser.cpp
// Owner : Masa Bayu
// Created: 2026-05-25
// Repo   : https://github.com/qxengine/qxengine-core
// Purpose: Minimal dependency-free JSON parser for QXManifest documents.
// =============================================================================

#include "qx_manifest_internal.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

namespace {

struct JsonParser {
    const char *p;
    const char *end;

    explicit JsonParser(const char *s, uint32_t len) : p(s), end(s + len) {}

    void skip_ws() {
        while (p < end && static_cast<unsigned char>(*p) <= 0x20u) ++p;
    }

    bool consume(char c) {
        skip_ws();
        if (p < end && *p == c) { ++p; return true; }
        return false;
    }

    bool read_string(char *buf, size_t buf_len) {
        skip_ws();
        if (p >= end || *p != '"') return false;
        ++p;
        size_t i = 0;
        while (p < end && *p != '"') {
            if (*p == '\\') {
                ++p;
                if (p >= end) return false;
                char esc = *p++;
                if (i < buf_len - 1) {
                    switch (esc) {
                        case '"':  buf[i++] = '"';  break;
                        case '\\': buf[i++] = '\\'; break;
                        case '/':  buf[i++] = '/';  break;
                        case 'n':  buf[i++] = '\n'; break;
                        case 'r':  buf[i++] = '\r'; break;
                        case 't':  buf[i++] = '\t'; break;
                        default:   buf[i++] = esc;  break;
                    }
                }
            } else {
                if (i < buf_len - 1) buf[i++] = *p;
                ++p;
            }
        }
        if (p < end) ++p;
        buf[i] = '\0';
        return true;
    }

    bool read_number(double &out) {
        skip_ws();
        if (p >= end) return false;
        char *endptr = nullptr;
        out = std::strtod(p, &endptr);
        if (endptr == p) return false;
        p = endptr;
        return true;
    }

    bool read_bool(bool &out) {
        skip_ws();
        if (p + 4 <= end && std::strncmp(p, "true", 4) == 0) {
            out = true; p += 4; return true;
        }
        if (p + 5 <= end && std::strncmp(p, "false", 5) == 0) {
            out = false; p += 5; return true;
        }
        return false;
    }

    void skip_value() {
        skip_ws();
        if (p >= end) return;
        if (*p == '"') { char tmp[256]; read_string(tmp, sizeof(tmp)); return; }
        if (*p == '{') { skip_object(); return; }
        if (*p == '[') { skip_array();  return; }
        if (std::strncmp(p, "true",  4) == 0) { p += 4; return; }
        if (std::strncmp(p, "false", 5) == 0) { p += 5; return; }
        if (std::strncmp(p, "null",  4) == 0) { p += 4; return; }
        while (p < end && (std::isdigit(static_cast<unsigned char>(*p)) ||
               *p == '-' || *p == '+' || *p == '.' || *p == 'e' || *p == 'E')) ++p;
    }

    void skip_object() {
        consume('{');
        skip_ws();
        if (p < end && *p == '}') { ++p; return; }
        while (p < end) {
            char key[256]; read_string(key, sizeof(key));
            consume(':');
            skip_value();
            skip_ws();
            if (p >= end || *p != ',') break;
            ++p;
        }
        consume('}');
    }

    void skip_array() {
        consume('[');
        skip_ws();
        if (p < end && *p == ']') { ++p; return; }
        while (p < end) {
            skip_value();
            skip_ws();
            if (p >= end || *p != ',') break;
            ++p;
        }
        consume(']');
    }

    bool read_key(char *key_buf, size_t key_len) {
        skip_ws();
        if (p >= end || *p == '}') return false;
        return read_string(key_buf, key_len);
    }
};

static void parse_meta(JsonParser &jp, QXManifestMeta &m) {
    jp.consume('{');
    char key[128];
    while (jp.read_key(key, sizeof(key))) {
        jp.consume(':');
        if (std::strcmp(key, "spec_version") == 0)
            jp.read_string(m.spec_version, sizeof(m.spec_version));
        else if (std::strcmp(key, "generated_by") == 0)
            jp.read_string(m.generated_by, sizeof(m.generated_by));
        else if (std::strcmp(key, "validated_at") == 0)
            jp.read_string(m.validated_at, sizeof(m.validated_at));
        else
            jp.skip_value();
        jp.skip_ws();
        if (jp.p >= jp.end || *jp.p != ',') break;
        ++jp.p;
    }
    jp.consume('}');
}

static void parse_identity(JsonParser &jp, QXManifestIdentity &id) {
    jp.consume('{');
    char key[128];
    while (jp.read_key(key, sizeof(key))) {
        jp.consume(':');
        if (std::strcmp(key, "app_id") == 0)
            jp.read_string(id.app_id, sizeof(id.app_id));
        else if (std::strcmp(key, "app_name") == 0)
            jp.read_string(id.app_name, sizeof(id.app_name));
        else if (std::strcmp(key, "owner") == 0)
            jp.read_string(id.owner, sizeof(id.owner));
        else if (std::strcmp(key, "version") == 0)
            jp.read_string(id.version, sizeof(id.version));
        else if (std::strcmp(key, "created") == 0)
            jp.read_string(id.created, sizeof(id.created));
        else if (std::strcmp(key, "platform") == 0)
            jp.read_string(id.platform, sizeof(id.platform));
        else if (std::strcmp(key, "minimum_engine_version") == 0)
            jp.read_string(id.minimum_engine_version, sizeof(id.minimum_engine_version));
        else
            jp.skip_value();
        jp.skip_ws();
        if (jp.p >= jp.end || *jp.p != ',') break;
        ++jp.p;
    }
    jp.consume('}');
}

static void parse_law_compliance(JsonParser &jp, QXManifestLawCompliance &lc) {
    jp.consume('{');
    char key[128];
    while (jp.read_key(key, sizeof(key))) {
        jp.consume(':');
        bool val = false;
        if (std::strcmp(key, "law1_pattern") == 0)     { jp.read_bool(val); lc.law1_pattern     = val ? QX_TRUE : QX_FALSE; }
        else if (std::strcmp(key, "law2_limit") == 0)       { jp.read_bool(val); lc.law2_limit       = val ? QX_TRUE : QX_FALSE; }
        else if (std::strcmp(key, "law3_pairs") == 0)       { jp.read_bool(val); lc.law3_pairs       = val ? QX_TRUE : QX_FALSE; }
        else if (std::strcmp(key, "law4_equilibrium") == 0) { jp.read_bool(val); lc.law4_equilibrium = val ? QX_TRUE : QX_FALSE; }
        else if (std::strcmp(key, "law5_knowledge") == 0)   { jp.read_bool(val); lc.law5_knowledge   = val ? QX_TRUE : QX_FALSE; }
        else if (std::strcmp(key, "law6_ethics") == 0)      { jp.read_bool(val); lc.law6_ethics      = val ? QX_TRUE : QX_FALSE; }
        else if (std::strcmp(key, "law7_creativity") == 0)  { jp.read_bool(val); lc.law7_creativity  = val ? QX_TRUE : QX_FALSE; }
        else if (std::strcmp(key, "law8_economy") == 0)     { jp.read_bool(val); lc.law8_economy     = val ? QX_TRUE : QX_FALSE; }
        else jp.skip_value();
        jp.skip_ws();
        if (jp.p >= jp.end || *jp.p != ',') break;
        ++jp.p;
    }
    jp.consume('}');
    lc.all_laws_active = (lc.law1_pattern && lc.law2_limit && lc.law3_pairs &&
                          lc.law4_equilibrium && lc.law5_knowledge && lc.law6_ethics &&
                          lc.law7_creativity && lc.law8_economy) ? QX_TRUE : QX_FALSE;
}

static void parse_segment(JsonParser &jp, QXManifestSegment &seg) {
    jp.consume('{');
    char key[128];
    while (jp.read_key(key, sizeof(key))) {
        jp.consume(':');
        double dv = 0.0;
        if (std::strcmp(key, "id") == 0)
            jp.read_string(seg.id, sizeof(seg.id));
        else if (std::strcmp(key, "name") == 0)
            jp.read_string(seg.name, sizeof(seg.name));
        else if (std::strcmp(key, "budget_percent") == 0)
            { jp.read_number(dv); seg.budget_percent = dv; }
        else if (std::strcmp(key, "soft_limit_mb") == 0)
            { jp.read_number(dv); seg.soft_limit_mb = dv; }
        else if (std::strcmp(key, "hard_limit_mb") == 0)
            { jp.read_number(dv); seg.hard_limit_mb = dv; }
        else
            jp.skip_value();
        jp.skip_ws();
        if (jp.p >= jp.end || *jp.p != ',') break;
        ++jp.p;
    }
    jp.consume('}');
}

static void parse_limit(JsonParser &jp, QXManifestLimit &lim) {
    jp.consume('{');
    char key[128];
    while (jp.read_key(key, sizeof(key))) {
        jp.consume(':');
        double dv = 0.0;
        if (std::strcmp(key, "memory_soft_limit_mb") == 0)
            { jp.read_number(dv); lim.memory_soft_limit_mb = dv; }
        else if (std::strcmp(key, "memory_hard_limit_mb") == 0)
            { jp.read_number(dv); lim.memory_hard_limit_mb = dv; }
        else if (std::strcmp(key, "segments") == 0) {
            jp.consume('[');
            jp.skip_ws();
            while (jp.p < jp.end && *jp.p != ']') {
                if (lim.segment_count < QX_SEGMENT_COUNT)
                    parse_segment(jp, lim.segments[lim.segment_count++]);
                else
                    jp.skip_value();
                jp.skip_ws();
                if (jp.p >= jp.end || *jp.p != ',') break;
                ++jp.p;
                jp.skip_ws();
            }
            jp.consume(']');
        } else
            jp.skip_value();
        jp.skip_ws();
        if (jp.p >= jp.end || *jp.p != ',') break;
        ++jp.p;
    }
    jp.consume('}');
}

static void parse_ethics(JsonParser &jp, QXManifestEthics &e) {
    jp.consume('{');
    char key[128];
    while (jp.read_key(key, sizeof(key))) {
        jp.consume(':');
        bool val = false;
        if (std::strcmp(key, "dark_patterns_prohibited") == 0)
            { jp.read_bool(val); e.dark_patterns_prohibited   = val ? QX_TRUE : QX_FALSE; }
        else if (std::strcmp(key, "deceptive_flows_prohibited") == 0)
            { jp.read_bool(val); e.deceptive_flows_prohibited = val ? QX_TRUE : QX_FALSE; }
        else if (std::strcmp(key, "manipulative_ux_prohibited") == 0)
            { jp.read_bool(val); e.manipulative_ux_prohibited = val ? QX_TRUE : QX_FALSE; }
        else if (std::strcmp(key, "privacy_first_design") == 0)
            { jp.read_bool(val); e.privacy_first_design       = val ? QX_TRUE : QX_FALSE; }
        else if (std::strcmp(key, "transparent_data_usage") == 0)
            { jp.read_bool(val); e.transparent_data_usage     = val ? QX_TRUE : QX_FALSE; }
        else jp.skip_value();
        jp.skip_ws();
        if (jp.p >= jp.end || *jp.p != ',') break;
        ++jp.p;
    }
    jp.consume('}');
    e.is_fully_ethical = (e.dark_patterns_prohibited && e.deceptive_flows_prohibited &&
                          e.manipulative_ux_prohibited && e.privacy_first_design &&
                          e.transparent_data_usage) ? QX_TRUE : QX_FALSE;
}

static void parse_native_capability(JsonParser &jp, QXManifestNativeCapability &cap) {
    jp.consume('{');
    char key[128];
    while (jp.read_key(key, sizeof(key))) {
        jp.consume(':');
        bool val = false;
        if (std::strcmp(key, "id") == 0)
            jp.read_string(cap.id, sizeof(cap.id));
        else if (std::strcmp(key, "name") == 0)
            jp.read_string(cap.name, sizeof(cap.name));
        else if (std::strcmp(key, "declared") == 0)
            { jp.read_bool(val); cap.declared = val ? QX_TRUE : QX_FALSE; }
        else jp.skip_value();
        jp.skip_ws();
        if (jp.p >= jp.end || *jp.p != ',') break;
        ++jp.p;
    }
    jp.consume('}');
}

static void parse_creativity(JsonParser &jp, QXManifestCreativity &c) {
    jp.consume('{');
    char key[128];
    while (jp.read_key(key, sizeof(key))) {
        jp.consume(':');
        bool bval = false; double dval = 0.0;
        if (std::strcmp(key, "native_first_policy") == 0)
            { jp.read_bool(bval); c.native_first_policy = bval ? QX_TRUE : QX_FALSE; }
        else if (std::strcmp(key, "native_utilisation_target") == 0)
            { jp.read_number(dval); c.native_utilisation_target = dval; }
        else if (std::strcmp(key, "native_capabilities") == 0) {
            jp.consume('[');
            jp.skip_ws();
            while (jp.p < jp.end && *jp.p != ']') {
                if (c.native_capability_count < QX_MANIFEST_CAPABILITY_CAP)
                    parse_native_capability(jp, c.native_capabilities[c.native_capability_count++]);
                else
                    jp.skip_value();
                jp.skip_ws();
                if (jp.p >= jp.end || *jp.p != ',') break;
                ++jp.p;
                jp.skip_ws();
            }
            jp.consume(']');
        } else jp.skip_value();
        jp.skip_ws();
        if (jp.p >= jp.end || *jp.p != ',') break;
        ++jp.p;
    }
    jp.consume('}');
}

static void parse_knowledge(JsonParser &jp, QXManifestKnowledge &k) {
    jp.consume('{');
    char key[128];
    while (jp.read_key(key, sizeof(key))) {
        jp.consume(':');
        double dval = 0.0; bool bval = false;
        if (std::strcmp(key, "snapshot_interval_seconds") == 0)
            { jp.read_number(dval); k.snapshot_interval_seconds = dval; }
        else if (std::strcmp(key, "min_knowledge_score") == 0)
            { jp.read_number(dval); k.min_knowledge_score = dval; }
        else if (std::strcmp(key, "telemetry_enabled") == 0)
            { jp.read_bool(bval); k.telemetry_enabled = bval ? QX_TRUE : QX_FALSE; }
        else jp.skip_value();
        jp.skip_ws();
        if (jp.p >= jp.end || *jp.p != ',') break;
        ++jp.p;
    }
    jp.consume('}');
}

static void parse_economy(JsonParser &jp, QXManifestEconomy &e) {
    jp.consume('{');
    char key[128];
    while (jp.read_key(key, sizeof(key))) {
        jp.consume(':');
        double dval = 0.0;
        if (std::strcmp(key, "network_budget_mb_per_session") == 0)
            { jp.read_number(dval); e.network_budget_mb_per_session = dval; }
        else if (std::strcmp(key, "battery_drain_max_pct_per_10min") == 0)
            { jp.read_number(dval); e.battery_drain_max_pct_per_10min = dval; }
        else if (std::strcmp(key, "commerce_model") == 0)
            jp.read_string(e.commerce_model, sizeof(e.commerce_model));
        else if (std::strcmp(key, "network_redundancy_max_pct") == 0)
            { jp.read_number(dval); e.network_redundancy_max_pct = dval; }
        else jp.skip_value();
        jp.skip_ws();
        if (jp.p >= jp.end || *jp.p != ',') break;
        ++jp.p;
    }
    jp.consume('}');
}

static void parse_verticals(JsonParser &jp, QXManifestVerticals &v) {
    jp.consume('{');
    char key[128];
    while (jp.read_key(key, sizeof(key))) {
        jp.consume(':');
        if (std::strcmp(key, "primary") == 0)
            jp.read_string(v.primary, sizeof(v.primary));
        else if (std::strcmp(key, "secondary") == 0) {
            jp.consume('[');
            jp.skip_ws();
            while (jp.p < jp.end && *jp.p != ']') {
                if (v.secondary_count < QX_MANIFEST_VERTICAL_SEC_MAX)
                    jp.read_string(v.secondary[v.secondary_count++],
                                   QX_MANIFEST_VERTICAL_MAX);
                else
                    jp.skip_value();
                jp.skip_ws();
                if (jp.p >= jp.end || *jp.p != ',') break;
                ++jp.p;
                jp.skip_ws();
            }
            jp.consume(']');
        } else jp.skip_value();
        jp.skip_ws();
        if (jp.p >= jp.end || *jp.p != ',') break;
        ++jp.p;
    }
    jp.consume('}');
}

} // namespace

bool qx_manifest_parse_json(const char *json, uint32_t len, QXManifest_s *m) {
    JsonParser jp(json, len);
    jp.skip_ws();
    if (!jp.consume('{')) return false;
    char key[128];
    while (jp.read_key(key, sizeof(key))) {
        jp.consume(':');
        if      (std::strcmp(key, "meta")           == 0) parse_meta(jp, m->meta);
        else if (std::strcmp(key, "identity")       == 0) parse_identity(jp, m->identity);
        else if (std::strcmp(key, "law_compliance") == 0) parse_law_compliance(jp, m->law_compliance);
        else if (std::strcmp(key, "limit")          == 0) parse_limit(jp, m->limit);
        else if (std::strcmp(key, "ethics")         == 0) parse_ethics(jp, m->ethics);
        else if (std::strcmp(key, "creativity")     == 0) parse_creativity(jp, m->creativity);
        else if (std::strcmp(key, "knowledge")      == 0) parse_knowledge(jp, m->knowledge);
        else if (std::strcmp(key, "economy")        == 0) parse_economy(jp, m->economy);
        else if (std::strcmp(key, "verticals")      == 0) parse_verticals(jp, m->verticals);
        else jp.skip_value();
        jp.skip_ws();
        if (jp.p >= jp.end || *jp.p != ',') break;
        ++jp.p;
    }
    jp.consume('}');
    return true;
}
