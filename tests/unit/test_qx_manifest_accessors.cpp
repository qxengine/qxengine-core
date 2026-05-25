// =============================================================================
// test_qx_manifest_accessors.cpp
// QXEngine Core — QXManifest Scalar and JSON Accessor Tests
// =============================================================================
// Owner  : Masa Bayu
// Created: 2026-05-24
// Repo   : https://github.com/qxengine/qxengine-core
// Description: Split accessor tests from test_qx_manifest.cpp.
// =============================================================================

#include <gtest/gtest.h>
#include <vector>

#include "qxengine/core/qx_constants.h"
#include "qxengine/core/qx_error.h"
#include "qxengine/core/qx_types.h"
#include "qxengine/manifest/qx_manifest.h"

namespace {

std::string make_valid_json() {
    return R"({
  "meta": { "spec_version": "1.0.0", "generated_by": "qxengine-cli", "validated_at": "2026-05-24" },
  "identity": {
    "app_id": "com.example.qxtest", "app_name": "QX Test App",
    "owner": "Masa Bayu", "version": "1.0.0", "created": "2026-05-24",
    "platform": "ios", "minimum_engine_version": "1.0.0"
  },
  "law_compliance": {
    "law1_pattern":true,"law2_limit":true,"law3_pairs":true,
    "law4_equilibrium":true,"law5_knowledge":true,"law6_ethics":true,
    "law7_creativity":true,"law8_economy":true
  },
  "limit": {
    "memory_soft_limit_mb": 128.0, "memory_hard_limit_mb": 256.0,
    "segments": [
      {"id":"QLM-CORE","name":"Core","budget_percent":20.0,"soft_limit_mb":25.6,"hard_limit_mb":51.2},
      {"id":"QLM-UI","name":"UI","budget_percent":15.0,"soft_limit_mb":19.2,"hard_limit_mb":38.4},
      {"id":"QLM-DATA","name":"Data","budget_percent":15.0,"soft_limit_mb":19.2,"hard_limit_mb":38.4},
      {"id":"QLM-IMG","name":"Images","budget_percent":12.0,"soft_limit_mb":15.36,"hard_limit_mb":30.72},
      {"id":"QLM-NET","name":"Network","budget_percent":10.0,"soft_limit_mb":12.8,"hard_limit_mb":25.6},
      {"id":"QLM-AI","name":"AI","budget_percent":10.0,"soft_limit_mb":12.8,"hard_limit_mb":25.6},
      {"id":"QLM-SEC","name":"Security","budget_percent":8.0,"soft_limit_mb":10.24,"hard_limit_mb":20.48},
      {"id":"QLM-LOG","name":"Logging","budget_percent":5.0,"soft_limit_mb":6.4,"hard_limit_mb":12.8},
      {"id":"QLM-TEMP","name":"Temp","budget_percent":5.0,"soft_limit_mb":6.4,"hard_limit_mb":12.8}
    ]
  },
  "ethics": {
    "dark_patterns_prohibited":true,"deceptive_flows_prohibited":true,
    "manipulative_ux_prohibited":true,"privacy_first_design":true,
    "transparent_data_usage":true
  },
  "creativity": {
    "native_first_policy":true,"native_utilisation_target":0.8,
    "native_capabilities":[{"id":"camera","name":"Camera","declared":true}]
  },
  "knowledge": {"snapshot_interval_seconds":5.0,"min_knowledge_score":90.0,"telemetry_enabled":true},
  "economy": {
    "network_budget_mb_per_session":100.0,"battery_drain_max_pct_per_10min":5.0,
    "commerce_model":"freemium","network_redundancy_max_pct":5.0
  },
  "verticals": {"primary":"productivity","secondary":["edtech"]}
})";
}

class QXManifestAccessorTest : public ::testing::Test {
protected:
    QXManifestHandle manifest = nullptr;

    void TearDown() override {
        qx_manifest_destroy(manifest);
        manifest = nullptr;
    }

    void parse_valid() {
        const std::string json = make_valid_json();
        QXManifestValidationResult result{};
        ASSERT_EQ(qx_manifest_parse(json.c_str(),
                                    static_cast<uint32_t>(json.size()),
                                    &manifest, &result), QX_OK);
        ASSERT_EQ(result.is_valid, QX_TRUE);
    }
};

} // namespace

TEST_F(QXManifestAccessorTest, AppIdAccessorReturnsCorrectValue) {
    parse_valid();
    char buf[QX_MANIFEST_APP_ID_MAX]{};
    ASSERT_EQ(qx_manifest_app_id(manifest, buf), QX_OK);
    EXPECT_STREQ(buf, "com.example.qxtest");
}

TEST_F(QXManifestAccessorTest, ScalarNullCasesReturnErrors) {
    char app_id[QX_MANIFEST_APP_ID_MAX]{};
    char platform[QX_MANIFEST_PLATFORM_MAX]{};
    double mb = 0.0;
    QXBool flag = QX_FALSE;

    EXPECT_EQ(qx_manifest_app_id(nullptr, app_id), QX_ERR_NULL_HANDLE);
    parse_valid();
    EXPECT_EQ(qx_manifest_app_id(manifest, nullptr), QX_ERR_NULL_HANDLE);
    EXPECT_EQ(qx_manifest_platform(nullptr, platform), QX_ERR_NULL_HANDLE);
    EXPECT_EQ(qx_manifest_soft_limit_mb(nullptr, &mb), QX_ERR_NULL_HANDLE);
    EXPECT_EQ(qx_manifest_hard_limit_mb(nullptr, &mb), QX_ERR_NULL_HANDLE);
    EXPECT_EQ(qx_manifest_all_laws_active(nullptr, &flag), QX_ERR_NULL_HANDLE);
    EXPECT_EQ(qx_manifest_is_fully_ethical(nullptr, &flag), QX_ERR_NULL_HANDLE);
}

TEST_F(QXManifestAccessorTest, ScalarAccessorsReturnExpectedValues) {
    parse_valid();
    char platform[QX_MANIFEST_PLATFORM_MAX]{};
    double soft = 0.0;
    double hard = 0.0;
    QXBool active = QX_FALSE;
    QXBool ethical = QX_FALSE;

    ASSERT_EQ(qx_manifest_platform(manifest, platform), QX_OK);
    ASSERT_EQ(qx_manifest_soft_limit_mb(manifest, &soft), QX_OK);
    ASSERT_EQ(qx_manifest_hard_limit_mb(manifest, &hard), QX_OK);
    ASSERT_EQ(qx_manifest_all_laws_active(manifest, &active), QX_OK);
    ASSERT_EQ(qx_manifest_is_fully_ethical(manifest, &ethical), QX_OK);

    EXPECT_STREQ(platform, "ios");
    EXPECT_DOUBLE_EQ(soft, 128.0);
    EXPECT_DOUBLE_EQ(hard, 256.0);
    EXPECT_EQ(active, QX_TRUE);
    EXPECT_EQ(ethical, QX_TRUE);
}

TEST_F(QXManifestAccessorTest, JsonSerialisationSucceeds) {
    parse_valid();
    uint32_t sz = 0;
    ASSERT_EQ(qx_manifest_json_size(manifest, &sz), QX_OK);
    ASSERT_GT(sz, 0u);

    std::vector<char> buf(sz, '\0');
    uint32_t written = 0;
    EXPECT_EQ(qx_manifest_to_json(manifest, buf.data(), sz, &written), QX_OK);
    EXPECT_GT(written, 0u);
    EXPECT_LT(written, sz);
}

TEST_F(QXManifestAccessorTest, JsonSerialisationErrorCasesReturnErrors) {
    char buf[256]{};
    char tiny[4]{};
    uint32_t written = 0;
    uint32_t sz = 0;

    EXPECT_EQ(qx_manifest_json_size(nullptr, &sz), QX_ERR_NULL_HANDLE);
    EXPECT_EQ(qx_manifest_to_json(nullptr, buf, sizeof(buf), &written),
              QX_ERR_NULL_HANDLE);

    parse_valid();
    EXPECT_EQ(qx_manifest_to_json(manifest, nullptr, 256, &written),
              QX_ERR_NULL_HANDLE);
    EXPECT_EQ(qx_manifest_to_json(manifest, tiny, sizeof(tiny), &written),
              QX_ERR_BUFFER_TOO_SMALL);
}

TEST_F(QXManifestAccessorTest, RoundTripJsonReValidatesSuccessfully) {
    parse_valid();
    uint32_t sz = 0;
    ASSERT_EQ(qx_manifest_json_size(manifest, &sz), QX_OK);
    std::vector<char> buf(sz, '\0');
    ASSERT_EQ(qx_manifest_to_json(manifest, buf.data(), sz, nullptr), QX_OK);

    QXManifestHandle copy = nullptr;
    QXManifestValidationResult result{};
    EXPECT_EQ(qx_manifest_parse(buf.data(), sz - 1u, &copy, &result), QX_OK);
    EXPECT_EQ(result.is_valid, QX_TRUE);
    qx_manifest_destroy(copy);
}
