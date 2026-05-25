// =============================================================================
// test_qx_engine_constitutional.cpp
// QXEngine Core – Engine and Constitutional Memory Integration
// =============================================================================
// Owner : Masa Bayu
// Created: 2026-05-25
// Repo   : https://github.com/qxengine/qxengine-core
// Path   : tests/integration/test_qx_engine_constitutional.cpp
// License: Apache 2.0
//

#include <gtest/gtest.h>

#include "qx_engine_internal.h"

#include <string>

namespace {

std::string make_manifest() {
    return R"({
  "meta": { "spec_version": "1.0.0", "generated_by": "qxengine-test", "validated_at": "2026-05-25" },
  "identity": {
    "app_id": "com.qxengine.constitutional",
    "app_name": "QX Constitutional Test",
    "owner": "Masa Bayu",
    "version": "1.0.0",
    "created": "2026-05-25",
    "platform": "android",
    "minimum_engine_version": "1.0.0"
  },
  "law_compliance": {
    "law1_pattern": true, "law2_limit": true, "law3_pairs": true,
    "law4_equilibrium": true, "law5_knowledge": true,
    "law6_ethics": true, "law7_creativity": true, "law8_economy": true
  },
  "limit": {
    "memory_soft_limit_mb": 128.0,
    "memory_hard_limit_mb": 256.0,
    "segments": [
      { "id": "QLM-CORE", "name": "Core", "budget_percent": 20.0, "soft_limit_mb": 25.6, "hard_limit_mb": 51.2 },
      { "id": "QLM-UI", "name": "UI", "budget_percent": 15.0, "soft_limit_mb": 19.2, "hard_limit_mb": 38.4 },
      { "id": "QLM-DATA", "name": "Data", "budget_percent": 15.0, "soft_limit_mb": 19.2, "hard_limit_mb": 38.4 },
      { "id": "QLM-IMG", "name": "Images", "budget_percent": 12.0, "soft_limit_mb": 15.36, "hard_limit_mb": 30.72 },
      { "id": "QLM-NET", "name": "Network", "budget_percent": 10.0, "soft_limit_mb": 12.8, "hard_limit_mb": 25.6 },
      { "id": "QLM-AI", "name": "AI", "budget_percent": 10.0, "soft_limit_mb": 12.8, "hard_limit_mb": 25.6 },
      { "id": "QLM-SEC", "name": "Security", "budget_percent": 8.0, "soft_limit_mb": 10.24, "hard_limit_mb": 20.48 },
      { "id": "QLM-LOG", "name": "Log", "budget_percent": 5.0, "soft_limit_mb": 6.4, "hard_limit_mb": 12.8 },
      { "id": "QLM-TEMP", "name": "Temp", "budget_percent": 5.0, "soft_limit_mb": 6.4, "hard_limit_mb": 12.8 }
    ]
  },
  "ethics": {
    "dark_patterns_prohibited": true, "deceptive_flows_prohibited": true,
    "manipulative_ux_prohibited": true, "privacy_first_design": true,
    "transparent_data_usage": true
  },
  "creativity": {
    "native_first_policy": true,
    "native_utilisation_target": 0.8,
    "native_capabilities": [{ "id": "camera", "name": "Camera", "declared": true }]
  },
  "knowledge": {
    "snapshot_interval_seconds": 5.0,
    "min_knowledge_score": 90.0,
    "telemetry_enabled": true
  },
  "economy": {
    "network_budget_mb_per_session": 100.0,
    "battery_drain_max_pct_per_10min": 5.0,
    "commerce_model": "freemium",
    "network_redundancy_max_pct": 5.0
  },
  "verticals": { "primary": "productivity", "secondary": [] }
})";
}

QXEngineHandle create_engine() {
    static std::string json;
    json = make_manifest();
    QXEngineConfig cfg{};
    cfg.manifest_json = json.c_str();
    cfg.manifest_json_length = static_cast<uint32_t>(json.size());
    cfg.device_ram_bytes = 4ULL * 1024ULL * 1024ULL * 1024ULL;
    cfg.verify_integrity = QX_FALSE;
    cfg.verbose = QX_FALSE;

    QXEngineHandle engine = nullptr;
    EXPECT_EQ(qx_engine_create(&cfg, &engine), QX_OK);
    return engine;
}

} // namespace

TEST(QXEngineConstitutional, CreateInitializesMemlocBridge) {
    QXEngineHandle engine = create_engine();
    ASSERT_NE(engine, nullptr);

    const auto* e = static_cast<QXEngine_s*>(engine);
    EXPECT_EQ(e->memloc_bridge.initialized, QX_TRUE);
    EXPECT_GE(qx_bridge_health_score(&e->memloc_bridge), QX_SCORE_SOVEREIGN);
    EXPECT_EQ(qx_bridge_certification(&e->memloc_bridge),
              QX_CERTIFICATION_SOVEREIGN);
    const QXMemlocPool* pool = &e->memloc_bridge.authority.pool;
    const QXPoolSegmentRegion* img =
        qx_pool_segment_region(pool, QX_SEGMENT_IMG);
    const QXPoolSegmentRegion* net =
        qx_pool_segment_region(pool, QX_SEGMENT_NET);
    const QXPoolSegmentRegion* ai =
        qx_pool_segment_region(pool, QX_SEGMENT_AI);
    ASSERT_NE(img, nullptr);
    ASSERT_NE(net, nullptr);
    ASSERT_NE(ai, nullptr);
    EXPECT_EQ(img->soft_limit_bytes, 26ULL * 1024ULL * 1024ULL);
    EXPECT_EQ(net->soft_limit_bytes, 26ULL * 1024ULL * 1024ULL);
    EXPECT_EQ(ai->soft_limit_bytes, 50ULL * 1024ULL * 1024ULL);

    qx_engine_destroy(engine);
}

TEST(QXEngineConstitutional, AllocationIsBoundToConstitutionalAuthority) {
    QXEngineHandle engine = create_engine();
    ASSERT_NE(engine, nullptr);
    ASSERT_EQ(qx_engine_start(engine), QX_OK);

    QXLeafHandle leaf = nullptr;
    ASSERT_EQ(qx_engine_allocate(engine, "engine.constitutional.leaf",
                                 QX_SEGMENT_IMG, QX_LEAF_CLASS_C,
                                 4096u, &leaf), QX_OK);
    ASSERT_NE(leaf, nullptr);

    char id[37]{};
    ASSERT_EQ(qx_leaf_id(leaf, id), QX_OK);

    const auto* e = static_cast<QXEngine_s*>(engine);
    EXPECT_EQ(e->memloc_bridge.total_allocations, 1u);

    ASSERT_EQ(qx_engine_deallocate(engine, id), QX_OK);
    EXPECT_EQ(e->memloc_bridge.total_deallocations, 1u);

    qx_engine_stop(engine);
    qx_engine_destroy(engine);
}

TEST(QXEngineConstitutional, EvaluationRunsConstitutionalCycle) {
    QXEngineHandle engine = create_engine();
    ASSERT_NE(engine, nullptr);
    ASSERT_EQ(qx_engine_start(engine), QX_OK);
    ASSERT_EQ(qx_engine_mark_capability_active(engine, "camera"), QX_OK);

    ASSERT_EQ(qx_engine_evaluate_now(engine), QX_OK);

    const auto* e = static_cast<QXEngine_s*>(engine);
    EXPECT_GE(e->memloc_bridge.last_report.cycle_number, 1u);
    EXPECT_GE(qx_bridge_health_score(&e->memloc_bridge), QX_SCORE_MINIMUM);
    EXPECT_LE(qx_bridge_health_score(&e->memloc_bridge), 100.0);

    qx_engine_stop(engine);
    qx_engine_destroy(engine);
}

TEST(QXEngineConstitutional, MemoryReadingForwardsPressureTier) {
    QXEngineHandle engine = create_engine();
    ASSERT_NE(engine, nullptr);
    ASSERT_EQ(qx_engine_start(engine), QX_OK);

    const uint64_t total = 4ULL * 1024ULL * 1024ULL * 1024ULL;
    const uint64_t available = total / 10ULL;
    ASSERT_EQ(qx_engine_feed_memory_reading(
                  engine, total - available, total, available), QX_OK);

    const auto* e = static_cast<QXEngine_s*>(engine);
    EXPECT_EQ(e->memloc_bridge.last_pressure_tier, QX_TIER_HIGH);

    qx_engine_stop(engine);
    qx_engine_destroy(engine);
}

TEST(QXEngineConstitutional, ExplicitSignalsForwardToBridge) {
    QXEngineHandle engine = create_engine();
    ASSERT_NE(engine, nullptr);
    ASSERT_EQ(qx_engine_start(engine), QX_OK);

    EXPECT_EQ(qx_engine_notify_lifecycle_change(engine, 1u), QX_OK);
    EXPECT_EQ(qx_engine_notify_lifecycle_change(engine, 0u), QX_OK);
    EXPECT_EQ(qx_engine_notify_screen_profile_change(
                  engine, "home-screen"), QX_OK);

    qx_engine_stop(engine);
    qx_engine_destroy(engine);
}
