/* =============================================================================
 * tests/integration/test_manifest_validation.cpp
 * QXEngine Core – Integration Tests: Manifest Validation
 * Repository : https://github.com/qxengine/qxengine-core
 * Owner      : Masa Bayu
 * Created    : 2026-05-24
 * Coverage   : Full MFT rule enforcement, parse round-trip, accessor
 *              integrity after validation, re-validation consistency
 * =========================================================================== */
#include <gtest/gtest.h>
#include <cstring>
#include <cstdint>
#include "qxengine/manifest/qx_manifest_types.h"
#include "qxengine/manifest/qx_manifest.h"

/* -------------------------------------------------------------------------- */
/* Helper – valid manifest JSON                                                */
/* -------------------------------------------------------------------------- */
static const char* valid_json(void)
{
    return R"({
  "meta": { "spec_version": "1.0.0", "generated_by": "qxengine-cli", "validated_at": "2026-05-24" },
  "identity": {
    "app_id": "com.qxengine.validation",
    "app_name": "QX Validation Test",
    "owner": "Masa Bayu",
    "version": "2.0.0",
    "created": "2026-05-24",
    "platform": "ios",
    "minimum_engine_version": "1.0.0"
  },
  "law_compliance": {
    "law1_pattern":     true,
    "law2_limit":       true,
    "law3_pairs":       true,
    "law4_equilibrium": true,
    "law5_knowledge":   true,
    "law6_ethics":      true,
    "law7_creativity":  true,
    "law8_economy":     true
  },
  "limit": {
    "memory_soft_limit_mb": 128.0,
    "memory_hard_limit_mb": 256.0,
    "segments": [
      { "id": "QLM-CORE",  "name": "Core Engine",  "budget_percent": 20.0, "soft_limit_mb": 25.6,  "hard_limit_mb": 51.2  },
      { "id": "QLM-UI",    "name": "UI",            "budget_percent": 15.0, "soft_limit_mb": 19.2,  "hard_limit_mb": 38.4  },
      { "id": "QLM-DATA",  "name": "Data",          "budget_percent": 15.0, "soft_limit_mb": 19.2,  "hard_limit_mb": 38.4  },
      { "id": "QLM-IMG",   "name": "Images",        "budget_percent": 12.0, "soft_limit_mb": 15.36, "hard_limit_mb": 30.72 },
      { "id": "QLM-NET",   "name": "Network",       "budget_percent": 10.0, "soft_limit_mb": 12.8,  "hard_limit_mb": 25.6  },
      { "id": "QLM-AI",    "name": "AI",            "budget_percent": 10.0, "soft_limit_mb": 12.8,  "hard_limit_mb": 25.6  },
      { "id": "QLM-SEC",   "name": "Security",      "budget_percent": 8.0,  "soft_limit_mb": 10.24, "hard_limit_mb": 20.48 },
      { "id": "QLM-LOG",   "name": "Logging",       "budget_percent": 5.0,  "soft_limit_mb": 6.4,   "hard_limit_mb": 12.8  },
      { "id": "QLM-TEMP",  "name": "Temp",          "budget_percent": 5.0,  "soft_limit_mb": 6.4,   "hard_limit_mb": 12.8  }
    ]
  },
  "ethics": {
    "dark_patterns_prohibited":   true,
    "deceptive_flows_prohibited": true,
    "manipulative_ux_prohibited": true,
    "privacy_first_design":       true,
    "transparent_data_usage":     true
  },
  "creativity": {
    "native_first_policy":       true,
    "native_utilisation_target": 0.8,
    "native_capabilities": [
      { "id": "camera", "name": "Camera", "declared": true }
    ]
  },
  "knowledge": {
    "snapshot_interval_seconds": 5.0,
    "min_knowledge_score":       90.0,
    "telemetry_enabled":         true
  },
  "economy": {
    "network_budget_mb_per_session":    100.0,
    "battery_drain_max_pct_per_10min":  5.0,
    "commerce_model":                   "freemium",
    "network_redundancy_max_pct":       5.0
  },
  "verticals": { "primary": "productivity", "secondary": [] }
})";
}

/* -------------------------------------------------------------------------- */
/* Helpers – invalid JSON variants (one violation each)                       */
/* -------------------------------------------------------------------------- */
static const char* json_blank_app_id(void)
{
    return R"({"meta":{"spec_version":"1.0.0"},"identity":{"app_id":"","app_name":"X","owner":"X","version":"1.0","created":"2026-01-01","platform":"android","minimum_engine_version":"1.0.0"}})";
}
static const char* json_bad_platform(void)
{
    return R"({"meta":{"spec_version":"1.0.0"},"identity":{"app_id":"com.x","app_name":"X","owner":"X","version":"1.0","created":"2026-01-01","platform":"dos","minimum_engine_version":"1.0.0"}})";
}
static const char* json_soft_limit_too_low(void)
{
    return R"({"meta":{"spec_version":"1.0.0"},"identity":{"app_id":"com.x","app_name":"X","owner":"X","version":"1.0","created":"2026-01-01","platform":"android","minimum_engine_version":"1.0.0"},"limit":{"memory_soft_limit_mb":10.0,"memory_hard_limit_mb":256.0,"segments":[]}})";
}
static const char* json_hard_below_soft(void)
{
    return R"({"meta":{"spec_version":"1.0.0"},"identity":{"app_id":"com.x","app_name":"X","owner":"X","version":"1.0","created":"2026-01-01","platform":"android","minimum_engine_version":"1.0.0"},"limit":{"memory_soft_limit_mb":256.0,"memory_hard_limit_mb":64.0,"segments":[]}})";
}
static const char* json_ethics_false(void)
{
    return R"({"meta":{"spec_version":"1.0.0"},"identity":{"app_id":"com.x","app_name":"X","owner":"X","version":"1.0","created":"2026-01-01","platform":"android","minimum_engine_version":"1.0.0"},"ethics":{"dark_patterns_prohibited":false,"deceptive_flows_prohibited":true,"manipulative_ux_prohibited":true,"privacy_first_design":true,"transparent_data_usage":true}})";
}
static const char* json_battery_too_high(void)
{
    return R"({"meta":{"spec_version":"1.0.0"},"identity":{"app_id":"com.x","app_name":"X","owner":"X","version":"1.0","created":"2026-01-01","platform":"android","minimum_engine_version":"1.0.0"},"economy":{"battery_drain_max_pct_per_10min":99.0,"network_budget_mb_per_session":10.0,"commerce_model":"free","network_redundancy_max_pct":5.0}})";
}

/* ========================================================================== */
/* Lifecycle / parse                                                           */
/* ========================================================================== */
TEST(QXManifestValidation, ParseValidJsonIsValid)
{
    QXManifestHandle h = nullptr;
    QXManifestValidationResult result;
    std::memset(&result, 0, sizeof(result));
    ASSERT_EQ(qx_manifest_parse(valid_json(), static_cast<uint32_t>(std::strlen(valid_json())), &h, &result), QX_OK);
    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(result.error_count, 0u);
    qx_manifest_destroy(h);
}
TEST(QXManifestValidation, ParseEmptyObjectIsInvalid)
{
    QXManifestHandle h = nullptr;
    QXManifestValidationResult result;
    std::memset(&result, 0, sizeof(result));
    qx_manifest_parse("{}", static_cast<uint32_t>(std::strlen("{}")), &h, &result);
    EXPECT_FALSE(result.is_valid);
    EXPECT_GT(result.error_count, 0u);
}
TEST(QXManifestValidation, ErrorCountNeverExceedsMax)
{
    QXManifestHandle h = nullptr;
    QXManifestValidationResult result;
    std::memset(&result, 0, sizeof(result));
    qx_manifest_parse("{}", static_cast<uint32_t>(std::strlen("{}")), &h, &result);
    EXPECT_LE(result.error_count, QX_MANIFEST_MAX_ERRORS);
}

/* ========================================================================== */
/* MFT rule violations                                                         */
/* ========================================================================== */
TEST(QXManifestValidation, MFT0002BlankAppIdIsInvalid)
{
    QXManifestHandle h = nullptr;
    QXManifestValidationResult result;
    std::memset(&result, 0, sizeof(result));
    qx_manifest_parse(json_blank_app_id(), static_cast<uint32_t>(std::strlen(json_blank_app_id())), &h, &result);
    EXPECT_FALSE(result.is_valid);
    EXPECT_GT(result.error_count, 0u);
}
TEST(QXManifestValidation, MFT0003BadPlatformIsInvalid)
{
    QXManifestHandle h = nullptr;
    QXManifestValidationResult result;
    std::memset(&result, 0, sizeof(result));
    qx_manifest_parse(json_bad_platform(), static_cast<uint32_t>(std::strlen(json_bad_platform())), &h, &result);
    EXPECT_FALSE(result.is_valid);
    EXPECT_GT(result.error_count, 0u);
}
TEST(QXManifestValidation, MFT0005SoftLimitTooLowIsInvalid)
{
    QXManifestHandle h = nullptr;
    QXManifestValidationResult result;
    std::memset(&result, 0, sizeof(result));
    qx_manifest_parse(json_soft_limit_too_low(), static_cast<uint32_t>(std::strlen(json_soft_limit_too_low())), &h, &result);
    EXPECT_FALSE(result.is_valid);
    EXPECT_GT(result.error_count, 0u);
}
TEST(QXManifestValidation, MFT0006HardBelowSoftIsInvalid)
{
    QXManifestHandle h = nullptr;
    QXManifestValidationResult result;
    std::memset(&result, 0, sizeof(result));
    qx_manifest_parse(json_hard_below_soft(), static_cast<uint32_t>(std::strlen(json_hard_below_soft())), &h, &result);
    EXPECT_FALSE(result.is_valid);
    EXPECT_GT(result.error_count, 0u);
}
TEST(QXManifestValidation, MFT0010EthicsFalseIsInvalid)
{
    QXManifestHandle h = nullptr;
    QXManifestValidationResult result;
    std::memset(&result, 0, sizeof(result));
    qx_manifest_parse(json_ethics_false(), static_cast<uint32_t>(std::strlen(json_ethics_false())), &h, &result);
    EXPECT_FALSE(result.is_valid);
    EXPECT_GT(result.error_count, 0u);
}
TEST(QXManifestValidation, MFT0013BatteryTooHighIsInvalid)
{
    QXManifestHandle h = nullptr;
    QXManifestValidationResult result;
    std::memset(&result, 0, sizeof(result));
    qx_manifest_parse(json_battery_too_high(), static_cast<uint32_t>(std::strlen(json_battery_too_high())), &h, &result);
    EXPECT_FALSE(result.is_valid);
    EXPECT_GT(result.error_count, 0u);
}

/* ========================================================================== */
/* Full validation round-trip                                                  */
/* ========================================================================== */
TEST(QXManifestValidation, RevalidateAfterParseIsConsistent)
{
    QXManifestHandle h = nullptr;
    QXManifestValidationResult r1;
    std::memset(&r1, 0, sizeof(r1));
    ASSERT_EQ(qx_manifest_parse(valid_json(), static_cast<uint32_t>(std::strlen(valid_json())), &h, &r1), QX_OK);

    QXManifestValidationResult r2;
    std::memset(&r2, 0, sizeof(r2));
    EXPECT_EQ(qx_manifest_validate(h, &r2), QX_OK);
    EXPECT_EQ(r1.is_valid,    r2.is_valid);
    EXPECT_EQ(r1.error_count, r2.error_count);
    qx_manifest_destroy(h);
}
TEST(QXManifestValidation, RevalidateNullHandleReturnsError)
{
    QXManifestValidationResult result;
    std::memset(&result, 0, sizeof(result));
    EXPECT_NE(qx_manifest_validate(nullptr, &result), QX_OK);
}
TEST(QXManifestValidation, SerialiseAndCheckNonEmpty)
{
    QXManifestHandle h = nullptr;
    QXManifestValidationResult result;
    std::memset(&result, 0, sizeof(result));
    ASSERT_EQ(qx_manifest_parse(valid_json(), static_cast<uint32_t>(std::strlen(valid_json())), &h, &result), QX_OK);
    char buf[8192];
    std::memset(buf, 0, sizeof(buf));
    EXPECT_EQ(qx_manifest_to_json(h, buf, static_cast<uint32_t>(sizeof(buf)), nullptr), QX_OK);
    EXPECT_GT(std::strlen(buf), 0u);
    qx_manifest_destroy(h);
}

/* ========================================================================== */
/* Accessor integrity after validation                                         */
/* ========================================================================== */
TEST(QXManifestValidation, AppIdMatchesAfterParse)
{
    QXManifestHandle h = nullptr;
    QXManifestValidationResult result;
    std::memset(&result, 0, sizeof(result));
    ASSERT_EQ(qx_manifest_parse(valid_json(), static_cast<uint32_t>(std::strlen(valid_json())), &h, &result), QX_OK);
    { char _aid[129]{}; qx_manifest_app_id(h, _aid); EXPECT_STREQ(_aid, "com.qxengine.validation"); };
    qx_manifest_destroy(h);
}
TEST(QXManifestValidation, PlatformMatchesAfterParse)
{
    QXManifestHandle h = nullptr;
    QXManifestValidationResult result;
    std::memset(&result, 0, sizeof(result));
    ASSERT_EQ(qx_manifest_parse(valid_json(), static_cast<uint32_t>(std::strlen(valid_json())), &h, &result), QX_OK);
    { char _plat[16]{}; qx_manifest_platform(h, _plat); EXPECT_STREQ(_plat, "ios"); };
    qx_manifest_destroy(h);
}
TEST(QXManifestValidation, LimitsMatchAfterParse)
{
    QXManifestHandle h = nullptr;
    QXManifestValidationResult result;
    std::memset(&result, 0, sizeof(result));
    ASSERT_EQ(qx_manifest_parse(valid_json(), static_cast<uint32_t>(std::strlen(valid_json())), &h, &result), QX_OK);
    { double _s=0.0; qx_manifest_soft_limit_mb(h, &_s); EXPECT_DOUBLE_EQ(_s, static_cast<double>(128u)); };
    { double _h=0.0; qx_manifest_hard_limit_mb(h, &_h); EXPECT_DOUBLE_EQ(_h, static_cast<double>(256u)); };
    qx_manifest_destroy(h);
}
TEST(QXManifestValidation, AllLawsActiveMatchesAfterParse)
{
    QXManifestHandle h = nullptr;
    QXManifestValidationResult result;
    std::memset(&result, 0, sizeof(result));
    ASSERT_EQ(qx_manifest_parse(valid_json(), static_cast<uint32_t>(std::strlen(valid_json())), &h, &result), QX_OK);
    { QXBool _al=QX_FALSE; qx_manifest_all_laws_active(h, &_al); EXPECT_EQ(_al, QX_TRUE); };
    qx_manifest_destroy(h);
}
