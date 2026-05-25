// =============================================================================
// QXEngine Core – tests/unit/test_qx_manifest.cpp
// Owner  : Masa Bayu
// Created: 2026-05-24
// Repo   : https://github.com/qxengine/qxengine-core
// Description: Unit tests for QXManifest – lifecycle, parse, validation,
//              MFT rule violations, block accessors, scalar helpers,
//              serialisation, and re-validation.
// =============================================================================

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>

#include "qxengine/manifest/qx_manifest.h"
#include "qxengine/manifest/qx_manifest_types.h"
#include "qxengine/core/qx_types.h"
#include "qxengine/core/qx_constants.h"
#include "qxengine/core/qx_error.h"

// =============================================================================
// MARK: – Minimal valid JSON helper
// =============================================================================

static std::string make_valid_json()
{
    return R"({

  "meta": { "spec_version": "1.0.0", "generated_by": "qxengine-cli", "validated_at": "2026-05-24" },
  "identity": {
    "app_id": "com.example.qxtest",
    "app_name": "QX Test App",
    "owner": "Masa Bayu",
    "version": "1.0.0",
    "created": "2026-05-24",
    "platform": "ios",
    "minimum_engine_version": "1.0.0"
  },
  "law_compliance": {
    "law1_pattern":true,"law2_limit":true,"law3_pairs":true,
    "law4_equilibrium":true,"law5_knowledge":true,"law6_ethics":true,
    "law7_creativity":true,"law8_economy":true
  },
  "limit": {
    "memory_soft_limit_mb": 128.0,
    "memory_hard_limit_mb": 256.0,
    "segments": [
      {"id":"QLM-CORE","name":"Core Engine","budget_percent":20.0,"soft_limit_mb":25.6,"hard_limit_mb":51.2},
      {"id":"QLM-UI",  "name":"UI",         "budget_percent":15.0,"soft_limit_mb":19.2,"hard_limit_mb":38.4},
      {"id":"QLM-DATA","name":"Data",       "budget_percent":15.0,"soft_limit_mb":19.2,"hard_limit_mb":38.4},
      {"id":"QLM-IMG", "name":"Images",     "budget_percent":12.0,"soft_limit_mb":15.36,"hard_limit_mb":30.72},
      {"id":"QLM-NET", "name":"Network",    "budget_percent":10.0,"soft_limit_mb":12.8,"hard_limit_mb":25.6},
      {"id":"QLM-AI",  "name":"AI",         "budget_percent":10.0,"soft_limit_mb":12.8,"hard_limit_mb":25.6},
      {"id":"QLM-SEC", "name":"Security",   "budget_percent":8.0,"soft_limit_mb":10.24,"hard_limit_mb":20.48},
      {"id":"QLM-LOG", "name":"Logging",    "budget_percent":5.0,"soft_limit_mb":6.4,"hard_limit_mb":12.8},
      {"id":"QLM-TEMP","name":"Temp",       "budget_percent":5.0,"soft_limit_mb":6.4,"hard_limit_mb":12.8}
    ]
  },
  "ethics": {
    "dark_patterns_prohibited":true,"deceptive_flows_prohibited":true,
    "manipulative_ux_prohibited":true,"privacy_first_design":true,
    "transparent_data_usage":true
  },
  "creativity": {
    "native_first_policy":true,"native_utilisation_target":0.8,
    "native_capabilities":[{"id":"camera","name":"Camera","declared":true},{"id":"microphone","name":"Microphone","declared":true}]
  },
  "knowledge": {"snapshot_interval_seconds":5.0,"min_knowledge_score":90.0,"telemetry_enabled":true},
  "economy": {
    "network_budget_mb_per_session":100.0,"battery_drain_max_pct_per_10min":5.0,
    "commerce_model":"freemium","network_redundancy_max_pct":5.0
  },
  "verticals": {"primary":"productivity","secondary":["edtech"]}
}})";
}

// =============================================================================
// MARK: – Fixture
// =============================================================================

class QXManifestTest : public ::testing::Test {
protected:
    QXManifestHandle manifest_ = nullptr;

    void TearDown() override {
        if (manifest_) {
            qx_manifest_destroy(manifest_);
            manifest_ = nullptr;
        }
    }

    // Parse the canonical valid JSON; asserts on failure.
    void parse_valid() {
        std::string json = make_valid_json();
        QXManifestValidationResult result{};
        QXResult rc = qx_manifest_parse(
            json.c_str(),
            static_cast<uint32_t>(json.size()),
            &manifest_,
            &result
        );
        ASSERT_EQ(rc, QX_OK);
        ASSERT_NE(manifest_, nullptr);
        ASSERT_EQ(result.is_valid, QX_TRUE);
    }
};

// =============================================================================
// MARK: – Lifecycle
// =============================================================================

TEST_F(QXManifestTest, ParseValidJsonSucceeds) {
    parse_valid();
    EXPECT_NE(manifest_, nullptr);
}

TEST_F(QXManifestTest, ParseNullJsonReturnsError) {
    QXManifestValidationResult result{};
    QXResult rc = qx_manifest_parse(nullptr, 0, &manifest_, &result);
    EXPECT_NE(rc, QX_OK);
    EXPECT_EQ(manifest_, nullptr);
}

TEST_F(QXManifestTest, ParseNullOutManifestReturnsError) {
    std::string json = make_valid_json();
    QXManifestValidationResult result{};
    QXResult rc = qx_manifest_parse(
        json.c_str(),
        static_cast<uint32_t>(json.size()),
        nullptr,
        &result
    );
    EXPECT_NE(rc, QX_OK);
}

TEST_F(QXManifestTest, ParseNullOutResultReturnsError) {
    std::string json = make_valid_json();
    QXResult rc = qx_manifest_parse(
        json.c_str(),
        static_cast<uint32_t>(json.size()),
        &manifest_,
        nullptr
    );
    EXPECT_NE(rc, QX_OK);
}

TEST_F(QXManifestTest, DestroyNullHandleIsSafe) {
    qx_manifest_destroy(nullptr); // must not crash
}

TEST_F(QXManifestTest, ParseEmptyJsonReturnsError) {
    const char *empty = "{}";
    QXManifestValidationResult result{};
    QXResult rc = qx_manifest_parse(
        empty,
        static_cast<uint32_t>(std::strlen(empty)),
        &manifest_,
        &result
    );
    EXPECT_NE(rc, QX_OK);
}

// =============================================================================
// MARK: – Validate
// =============================================================================

TEST_F(QXManifestTest, ValidateAfterParseSucceeds) {
    parse_valid();
    QXManifestValidationResult result{};
    QXResult rc = qx_manifest_validate(manifest_, &result);
    EXPECT_EQ(rc, QX_OK);
    EXPECT_EQ(result.is_valid, QX_TRUE);
    EXPECT_EQ(result.error_count, 0u);
}

TEST_F(QXManifestTest, ValidateNullHandleReturnsError) {
    QXManifestValidationResult result{};
    QXResult rc = qx_manifest_validate(nullptr, &result);
    EXPECT_EQ(rc, QX_ERR_NULL_HANDLE);
}

TEST_F(QXManifestTest, ValidateNullResultReturnsError) {
    parse_valid();
    QXResult rc = qx_manifest_validate(manifest_, nullptr);
    EXPECT_EQ(rc, QX_ERR_NULL_HANDLE);
}

// =============================================================================
// MARK: – MFT rule violations
// =============================================================================

TEST_F(QXManifestTest, MissingLawFlagFailsValidation) {
    // law1_pattern set to false – should fail MFT-0010
    std::string json = make_valid_json();
    // Replace law1_pattern true → false
    auto pos = json.find("\"law1_pattern\":true");
    ASSERT_NE(pos, std::string::npos);
    json.replace(pos, std::strlen("\"law1_pattern\":true"),
                 "\"law1_pattern\":false");

    QXManifestValidationResult result{};
    QXResult rc = qx_manifest_parse(
        json.c_str(),
        static_cast<uint32_t>(json.size()),
        &manifest_,
        &result
    );
    EXPECT_NE(rc, QX_OK);
    EXPECT_EQ(result.is_valid, QX_FALSE);
}

TEST_F(QXManifestTest, SoftLimitBelowMinimumFailsValidation) {
    // Set memorySoftLimitMB to 10 (below QX_MIN_SOFT_LIMIT_MB = 64)
    std::string json = make_valid_json();
    auto pos = json.find("\"memory_soft_limit_mb\": 128.0");
    ASSERT_NE(pos, std::string::npos);
    json.replace(pos, std::strlen("\"memory_soft_limit_mb\": 128.0"),
                 "\"memory_soft_limit_mb\": 10.0");

    QXManifestValidationResult result{};
    QXResult rc = qx_manifest_parse(
        json.c_str(),
        static_cast<uint32_t>(json.size()),
        &manifest_,
        &result
    );
    EXPECT_NE(rc, QX_OK);
    EXPECT_EQ(result.is_valid, QX_FALSE);
}

TEST_F(QXManifestTest, HardLimitBelowSoftLimitFailsValidation) {
    // Set memoryHardLimitMB below memorySoftLimitMB
    std::string json = make_valid_json();
    auto pos = json.find("\"memory_hard_limit_mb\": 256.0");
    ASSERT_NE(pos, std::string::npos);
    json.replace(pos, std::strlen("\"memory_hard_limit_mb\": 256.0"),
                 "\"memory_hard_limit_mb\": 64.0");

    QXManifestValidationResult result{};
    QXResult rc = qx_manifest_parse(
        json.c_str(),
        static_cast<uint32_t>(json.size()),
        &manifest_,
        &result
    );
    EXPECT_NE(rc, QX_OK);
    EXPECT_EQ(result.is_valid, QX_FALSE);
}

TEST_F(QXManifestTest, NativeFirstPolicyFalseFailsValidation) {
    std::string json = make_valid_json();
    auto pos = json.find("\"native_first_policy\":true");
    ASSERT_NE(pos, std::string::npos);
    json.replace(pos, std::strlen("\"native_first_policy\":true"),
                 "\"native_first_policy\":false");

    QXManifestValidationResult result{};
    QXResult rc = qx_manifest_parse(
        json.c_str(),
        static_cast<uint32_t>(json.size()),
        &manifest_,
        &result
    );
    EXPECT_NE(rc, QX_OK);
    EXPECT_EQ(result.is_valid, QX_FALSE);
}

TEST_F(QXManifestTest, EthicsFlagFalseFailsValidation) {
    std::string json = make_valid_json();
    auto pos = json.find("\"dark_patterns_prohibited\":true");
    ASSERT_NE(pos, std::string::npos);
    json.replace(pos, std::strlen("\"dark_patterns_prohibited\":true"),
                 "\"dark_patterns_prohibited\":false");

    QXManifestValidationResult result{};
    QXResult rc = qx_manifest_parse(
        json.c_str(),
        static_cast<uint32_t>(json.size()),
        &manifest_,
        &result
    );
    EXPECT_NE(rc, QX_OK);
    EXPECT_EQ(result.is_valid, QX_FALSE);
}

// =============================================================================
// MARK: – Block accessors
// =============================================================================

TEST_F(QXManifestTest, IdentityBlockReturnsCorrectAppId) {
    parse_valid();
    QXManifestIdentity id{};
    ASSERT_EQ(qx_manifest_identity(manifest_, &id), QX_OK);
    EXPECT_STREQ(id.app_id, "com.example.qxtest");
}

TEST_F(QXManifestTest, IdentityBlockReturnsCorrectPlatform) {
    parse_valid();
    QXManifestIdentity id{};
    ASSERT_EQ(qx_manifest_identity(manifest_, &id), QX_OK);
    EXPECT_STREQ(id.platform, "ios");
}

TEST_F(QXManifestTest, IdentityNullHandleReturnsError) {
    QXManifestIdentity id{};
    EXPECT_EQ(qx_manifest_identity(nullptr, &id), QX_ERR_NULL_HANDLE);
}

TEST_F(QXManifestTest, IdentityNullOutReturnsError) {
    parse_valid();
    EXPECT_EQ(qx_manifest_identity(manifest_, nullptr), QX_ERR_NULL_HANDLE);
}

TEST_F(QXManifestTest, LawComplianceAllActive) {
    parse_valid();
    QXManifestLawCompliance compliance{};
    ASSERT_EQ(qx_manifest_law_compliance(manifest_, &compliance), QX_OK);
    EXPECT_EQ(compliance.all_laws_active,    QX_TRUE);
    EXPECT_EQ(compliance.law1_pattern,       QX_TRUE);
    EXPECT_EQ(compliance.law2_limit,         QX_TRUE);
    EXPECT_EQ(compliance.law3_pairs,         QX_TRUE);
    EXPECT_EQ(compliance.law4_equilibrium,   QX_TRUE);
    EXPECT_EQ(compliance.law5_knowledge,     QX_TRUE);
    EXPECT_EQ(compliance.law6_ethics,        QX_TRUE);
    EXPECT_EQ(compliance.law7_creativity,    QX_TRUE);
    EXPECT_EQ(compliance.law8_economy,       QX_TRUE);
}

TEST_F(QXManifestTest, LawComplianceNullHandleReturnsError) {
    QXManifestLawCompliance compliance{};
    EXPECT_EQ(qx_manifest_law_compliance(nullptr, &compliance), QX_ERR_NULL_HANDLE);
}

TEST_F(QXManifestTest, LimitBlockSoftAndHard) {
    parse_valid();
    QXManifestLimit limit{};
    ASSERT_EQ(qx_manifest_limit(manifest_, &limit), QX_OK);
    EXPECT_DOUBLE_EQ(limit.memory_soft_limit_mb, 128.0);
    EXPECT_DOUBLE_EQ(limit.memory_hard_limit_mb, 256.0);
    EXPECT_EQ(limit.segment_count, static_cast<uint32_t>(QX_SEGMENT_COUNT));
}

TEST_F(QXManifestTest, LimitNullHandleReturnsError) {
    QXManifestLimit limit{};
    EXPECT_EQ(qx_manifest_limit(nullptr, &limit), QX_ERR_NULL_HANDLE);
}

TEST_F(QXManifestTest, SegmentAccessorCoreSegment) {
    parse_valid();
    QXManifestSegment seg{};
    ASSERT_EQ(qx_manifest_segment(manifest_, "QLM-CORE", &seg), QX_OK);
    EXPECT_STREQ(seg.id, "QLM-CORE");
    EXPECT_GT(seg.soft_limit_mb, 0.0);
    EXPECT_GT(seg.hard_limit_mb, seg.soft_limit_mb);
}

TEST_F(QXManifestTest, SegmentAccessorUnknownIdReturnsError) {
    parse_valid();
    QXManifestSegment seg{};
    QXResult rc = qx_manifest_segment(manifest_, "QLM-UNKNOWN", &seg);
    EXPECT_NE(rc, QX_OK);
}

TEST_F(QXManifestTest, SegmentAccessorNullHandleReturnsError) {
    QXManifestSegment seg{};
    EXPECT_EQ(qx_manifest_segment(nullptr, "QLM-CORE", &seg), QX_ERR_NULL_HANDLE);
}

TEST_F(QXManifestTest, EthicsBlockAllTrue) {
    parse_valid();
    QXManifestEthics ethics{};
    ASSERT_EQ(qx_manifest_ethics(manifest_, &ethics), QX_OK);
    EXPECT_EQ(ethics.is_fully_ethical,          QX_TRUE);
    EXPECT_EQ(ethics.dark_patterns_prohibited,   QX_TRUE);
    EXPECT_EQ(ethics.deceptive_flows_prohibited, QX_TRUE);
    EXPECT_EQ(ethics.manipulative_ux_prohibited, QX_TRUE);
    EXPECT_EQ(ethics.privacy_first_design,       QX_TRUE);
    EXPECT_EQ(ethics.transparent_data_usage,     QX_TRUE);
}

TEST_F(QXManifestTest, EthicsNullHandleReturnsError) {
    QXManifestEthics ethics{};
    EXPECT_EQ(qx_manifest_ethics(nullptr, &ethics), QX_ERR_NULL_HANDLE);
}

TEST_F(QXManifestTest, CreativityBlockNativeFirstPolicy) {
    parse_valid();
    QXManifestCreativity creativity{};
    ASSERT_EQ(qx_manifest_creativity(manifest_, &creativity), QX_OK);
    EXPECT_EQ(creativity.native_first_policy, QX_TRUE);
    EXPECT_EQ(creativity.native_capability_count, 2u);
}

TEST_F(QXManifestTest, CreativityNullHandleReturnsError) {
    QXManifestCreativity creativity{};
    EXPECT_EQ(qx_manifest_creativity(nullptr, &creativity), QX_ERR_NULL_HANDLE);
}

TEST_F(QXManifestTest, KnowledgeBlockValues) {
    parse_valid();
    QXManifestKnowledge knowledge{};
    ASSERT_EQ(qx_manifest_knowledge(manifest_, &knowledge), QX_OK);
    EXPECT_DOUBLE_EQ(knowledge.snapshot_interval_seconds, 5.0);
    EXPECT_DOUBLE_EQ(knowledge.min_knowledge_score, 90.0);
    EXPECT_EQ(knowledge.telemetry_enabled, QX_TRUE);
}

TEST_F(QXManifestTest, KnowledgeNullHandleReturnsError) {
    QXManifestKnowledge knowledge{};
    EXPECT_EQ(qx_manifest_knowledge(nullptr, &knowledge), QX_ERR_NULL_HANDLE);
}

TEST_F(QXManifestTest, EconomyBlockValues) {
    parse_valid();
    QXManifestEconomy economy{};
    ASSERT_EQ(qx_manifest_economy(manifest_, &economy), QX_OK);
    EXPECT_GT(economy.network_budget_mb_per_session, 0.0);
    EXPECT_GT(economy.battery_drain_max_pct_per_10min, 0.0);
    EXPECT_STREQ(economy.commerce_model, "freemium");
}

TEST_F(QXManifestTest, EconomyNullHandleReturnsError) {
    QXManifestEconomy economy{};
    EXPECT_EQ(qx_manifest_economy(nullptr, &economy), QX_ERR_NULL_HANDLE);
}

TEST_F(QXManifestTest, VerticalsBlockPrimary) {
    parse_valid();
    QXManifestVerticals verticals{};
    ASSERT_EQ(qx_manifest_verticals(manifest_, &verticals), QX_OK);
    EXPECT_STREQ(verticals.primary, "productivity");
}

TEST_F(QXManifestTest, VerticalsNullHandleReturnsError) {
    QXManifestVerticals verticals{};
    EXPECT_EQ(qx_manifest_verticals(nullptr, &verticals), QX_ERR_NULL_HANDLE);
}

TEST_F(QXManifestTest, MetaBlockSpecVersion) {
    parse_valid();
    QXManifestMeta meta{};
    ASSERT_EQ(qx_manifest_meta(manifest_, &meta), QX_OK);
    EXPECT_STREQ(meta.spec_version, "1.0.0");
}

TEST_F(QXManifestTest, MetaNullHandleReturnsError) {
    QXManifestMeta meta{};
    EXPECT_EQ(qx_manifest_meta(nullptr, &meta), QX_ERR_NULL_HANDLE);
}

