// =============================================================================
// QXEngine Core – tests/integration/test_full_evaluation_cycle.cpp
// Owner  : Masa Bayu
// Created: 2026-05-24
// Repo   : https://github.com/qxengine/qxengine-core
// Description: Integration tests for the full evaluation cycle –
//              engine create → alloc → law eval → pressure → destroy.
// =============================================================================

#include <gtest/gtest.h>
#include <cstring>
#include <cstdint>

#include "qxengine/qxengine.h"

// =============================================================================
// MARK: – Valid manifest JSON
// =============================================================================

static std::string make_valid_manifest_json()
{
    return R"({
  "meta": { "spec_version": "1.0.0", "generated_by": "qxengine-cli", "validated_at": "2026-05-24" },
  "identity": {
    "app_id": "com.qxengine.integration",
    "app_name": "QX Integration Test",
    "owner": "Masa Bayu",
    "version": "1.0.0",
    "created": "2026-05-24",
    "platform": "android",
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

// =============================================================================
// MARK: – Fixture
// =============================================================================

class QXFullCycleTest : public ::testing::Test {
protected:
    QXEngineHandle engine_ = nullptr;

    void SetUp() override {
        json_ = make_valid_manifest_json();

        QXEngineConfig cfg{};
        cfg.manifest_json        = json_.c_str();
        cfg.manifest_json_length = static_cast<uint32_t>(json_.size());
        cfg.device_ram_bytes     = 4ULL * 1024ULL * 1024ULL * 1024ULL; // 4 GiB
        cfg.verify_integrity     = QX_FALSE;
        cfg.verbose              = QX_FALSE;

        ASSERT_EQ(qx_engine_create(&cfg, &engine_), QX_OK);
        ASSERT_NE(engine_, nullptr);

        ASSERT_EQ(qx_engine_start(engine_), QX_OK);
    }

    void TearDown() override {
        if (engine_) {
            qx_engine_stop(engine_);
            qx_engine_destroy(engine_);
            engine_ = nullptr;
        }
    }

private:
    std::string json_;
};

// =============================================================================
// MARK: – Lifecycle
// =============================================================================

TEST_F(QXFullCycleTest, EngineCreatesAndStartsSuccessfully) {
    QXEngineState state{};
    ASSERT_EQ(qx_engine_state(engine_, &state), QX_OK);
    EXPECT_EQ(state, QX_ENGINE_STATE_RUNNING);
}

TEST_F(QXFullCycleTest, CreateNullConfigReturnsError) {
    QXEngineHandle h = nullptr;
    EXPECT_NE(qx_engine_create(nullptr, &h), QX_OK);
    EXPECT_EQ(h, nullptr);
}

TEST_F(QXFullCycleTest, CreateNullOutHandleReturnsError) {
    std::string json = make_valid_manifest_json();
    QXEngineConfig cfg{};
    cfg.manifest_json        = json.c_str();
    cfg.manifest_json_length = static_cast<uint32_t>(json.size());
    cfg.device_ram_bytes     = 4ULL * 1024ULL * 1024ULL * 1024ULL;
    EXPECT_NE(qx_engine_create(&cfg, nullptr), QX_OK);
}

TEST_F(QXFullCycleTest, DestroyNullHandleIsSafe) {
    qx_engine_destroy(nullptr); // must not crash
}

TEST_F(QXFullCycleTest, PauseAndResumeSucceed) {
    ASSERT_EQ(qx_engine_pause(engine_),  QX_OK);
    QXEngineState state{};
    ASSERT_EQ(qx_engine_state(engine_, &state), QX_OK);
    EXPECT_EQ(state, QX_ENGINE_STATE_PAUSED);

    ASSERT_EQ(qx_engine_resume(engine_), QX_OK);
    ASSERT_EQ(qx_engine_state(engine_,  &state), QX_OK);
    EXPECT_EQ(state, QX_ENGINE_STATE_RUNNING);
}

TEST_F(QXFullCycleTest, StateNullHandleReturnsError) {
    QXEngineState state{};
    EXPECT_EQ(qx_engine_state(nullptr, &state), QX_ERR_NULL_HANDLE);
}

// =============================================================================
// MARK: – Allocation / deallocation
// =============================================================================

TEST_F(QXFullCycleTest, AllocateAndDeallocateSucceeds) {
    QXLeafHandle leaf = nullptr;
    ASSERT_EQ(qx_engine_allocate(
        engine_, "integration.alloc.test", "QLM-CORE",
        QX_LEAF_CLASS_B, 64u * 1024u, &leaf), QX_OK);
    ASSERT_NE(leaf, nullptr);

    char id[37]{};
    ASSERT_EQ(qx_leaf_id(leaf, id), QX_OK);
    EXPECT_EQ(qx_engine_deallocate(engine_, id), QX_OK);
}

TEST_F(QXFullCycleTest, AllocateNullLabelReturnsError) {
    QXLeafHandle leaf = nullptr;
    EXPECT_NE(qx_engine_allocate(
        engine_, nullptr, "QLM-CORE",
        QX_LEAF_CLASS_A, 1024u, &leaf), QX_OK);
}

TEST_F(QXFullCycleTest, AllocateUnknownSegmentReturnsError) {
    QXLeafHandle leaf = nullptr;
    EXPECT_NE(qx_engine_allocate(
        engine_, "test.unknown.seg", "QLM-UNKNOWN",
        QX_LEAF_CLASS_A, 1024u, &leaf), QX_OK);
}

TEST_F(QXFullCycleTest, AllocateZeroSizeReturnsError) {
    QXLeafHandle leaf = nullptr;
    EXPECT_NE(qx_engine_allocate(
        engine_, "test.zero.size", "QLM-CORE",
        QX_LEAF_CLASS_A, 0u, &leaf), QX_OK);
}

TEST_F(QXFullCycleTest, DeallocateUnknownIdReturnsError) {
    EXPECT_NE(qx_engine_deallocate(engine_, "00000000-0000-0000-0000-000000000000"), QX_OK);
}

TEST_F(QXFullCycleTest, DoubleDeallocateReturnsError) {
    QXLeafHandle leaf = nullptr;
    ASSERT_EQ(qx_engine_allocate(
        engine_, "integration.double.free", "QLM-DATA",
        QX_LEAF_CLASS_C, 1024u, &leaf), QX_OK);

    char id[37]{};
    ASSERT_EQ(qx_leaf_id(leaf, id), QX_OK);
    ASSERT_EQ(qx_engine_deallocate(engine_, id), QX_OK);
    EXPECT_NE(qx_engine_deallocate(engine_, id), QX_OK);
}

TEST_F(QXFullCycleTest, TouchActiveLeafSucceeds) {
    QXLeafHandle leaf = nullptr;
    ASSERT_EQ(qx_engine_allocate(
        engine_, "integration.touch.test", "QLM-NET",
        QX_LEAF_CLASS_A, 512u, &leaf), QX_OK);

    char id[37]{};
    ASSERT_EQ(qx_leaf_id(leaf, id), QX_OK);
    EXPECT_EQ(qx_engine_touch(engine_, id), QX_OK);

    qx_engine_deallocate(engine_, id);
}

// =============================================================================
// MARK: – Evaluation
// =============================================================================

TEST_F(QXFullCycleTest, EvaluateNowSucceeds) {
    EXPECT_EQ(qx_engine_evaluate_now(engine_), QX_OK);
}

TEST_F(QXFullCycleTest, EvaluateNowNullHandleReturnsError) {
    EXPECT_EQ(qx_engine_evaluate_now(nullptr), QX_ERR_NULL_HANDLE);
}

TEST_F(QXFullCycleTest, MultipleEvaluationsSucceed) {
    for (int i = 0; i < 3; ++i)
        EXPECT_EQ(qx_engine_evaluate_now(engine_), QX_OK);
}

// =============================================================================
// MARK: – Memory pressure
// =============================================================================

TEST_F(QXFullCycleTest, NotifyPressureLevelNormalSucceeds) {
    EXPECT_EQ(qx_engine_notify_memory_pressure(engine_, 1u), QX_OK);
}

TEST_F(QXFullCycleTest, NotifyPressureLevelElevatedSucceeds) {
    EXPECT_EQ(qx_engine_notify_memory_pressure(engine_, 2u), QX_OK);
}

TEST_F(QXFullCycleTest, NotifyPressureLevelHighSucceeds) {
    EXPECT_EQ(qx_engine_notify_memory_pressure(engine_, 3u), QX_OK);
}

TEST_F(QXFullCycleTest, NotifyPressureLevelCriticalSucceeds) {
    // Critical pressure on empty memloc may return INSUFFICIENT_EVICTION
    QXResult rc = qx_engine_notify_memory_pressure(engine_, 4u);
    EXPECT_TRUE(rc == QX_OK || rc == QX_ERR_INSUFFICIENT_EVICTION);
}

TEST_F(QXFullCycleTest, NotifyPressureNullHandleReturnsError) {
    EXPECT_EQ(qx_engine_notify_memory_pressure(nullptr, 2u), QX_ERR_NULL_HANDLE);
}

// =============================================================================
// MARK: – Economy metrics
// =============================================================================

TEST_F(QXFullCycleTest, UpdateNetworkRedundancySucceeds) {
    EXPECT_EQ(qx_engine_update_network_redundancy(engine_, 3.5), QX_OK);
}

TEST_F(QXFullCycleTest, UpdateNetworkRedundancyNullHandleReturnsError) {
    EXPECT_EQ(qx_engine_update_network_redundancy(nullptr, 3.5), QX_ERR_NULL_HANDLE);
}

TEST_F(QXFullCycleTest, UpdateBatteryDrainSucceeds) {
    EXPECT_EQ(qx_engine_update_battery_drain(engine_, 2.0), QX_OK);
}

TEST_F(QXFullCycleTest, UpdateBatteryDrainNullHandleReturnsError) {
    EXPECT_EQ(qx_engine_update_battery_drain(nullptr, 2.0), QX_ERR_NULL_HANDLE);
}

// =============================================================================
// MARK: – OS memory feed
// =============================================================================

TEST_F(QXFullCycleTest, FeedMemoryReadingSucceeds) {
    EXPECT_EQ(qx_engine_feed_memory_reading(
        engine_,
        128ULL * 1024ULL * 1024ULL,   // 128 MiB resident
        4ULL  * 1024ULL * 1024ULL * 1024ULL, // 4 GiB total
        2ULL  * 1024ULL * 1024ULL * 1024ULL  // 2 GiB available
    ), QX_OK);
}

TEST_F(QXFullCycleTest, FeedMemoryReadingNullHandleReturnsError) {
    EXPECT_EQ(qx_engine_feed_memory_reading(nullptr, 1024u, 4096u, 2048u),
              QX_ERR_NULL_HANDLE);
}

// =============================================================================
// MARK: – Native capability
// =============================================================================

TEST_F(QXFullCycleTest, MarkCapabilityActiveSucceeds) {
    EXPECT_EQ(qx_engine_mark_capability_active(engine_, "camera"), QX_OK);
}

TEST_F(QXFullCycleTest, MarkCapabilityActiveNullHandleReturnsError) {
    EXPECT_EQ(qx_engine_mark_capability_active(nullptr, "camera"), QX_ERR_NULL_HANDLE);
}

TEST_F(QXFullCycleTest, MarkCapabilityActiveNullIdReturnsError) {
    EXPECT_EQ(qx_engine_mark_capability_active(engine_, nullptr), QX_ERR_NULL_HANDLE);
}

// =============================================================================
// MARK: – Full cycle: alloc → eval → pressure → dealloc
// =============================================================================

TEST_F(QXFullCycleTest, FullAllocEvalPressureDeallocCycle) {
    // Allocate across multiple segments
    QXLeafHandle l1 = nullptr, l2 = nullptr, l3 = nullptr;
    ASSERT_EQ(qx_engine_allocate(engine_, "cycle.leaf.one",   "QLM-CORE",
                                 QX_LEAF_CLASS_A, 32u * 1024u, &l1), QX_OK);
    ASSERT_EQ(qx_engine_allocate(engine_, "cycle.leaf.two",   "QLM-IMG",
                                 QX_LEAF_CLASS_B, 64u * 1024u, &l2), QX_OK);
    ASSERT_EQ(qx_engine_allocate(engine_, "cycle.leaf.three", "QLM-NET",
                                 QX_LEAF_CLASS_C, 16u * 1024u, &l3), QX_OK);

    // Feed a memory reading
    qx_engine_feed_memory_reading(engine_,
        128ULL * 1024ULL * 1024ULL,
        4ULL * 1024ULL * 1024ULL * 1024ULL,
        2ULL * 1024ULL * 1024ULL * 1024ULL);

    // Evaluate constitutional laws
    EXPECT_EQ(qx_engine_evaluate_now(engine_), QX_OK);

    // Simulate memory pressure
    EXPECT_EQ(qx_engine_notify_memory_pressure(engine_, 2u), QX_OK);

    // Touch an active leaf
    char id2[37]{};
    ASSERT_EQ(qx_leaf_id(l2, id2), QX_OK);
    qx_engine_touch(engine_, id2);

    // Evaluate again after pressure
    EXPECT_EQ(qx_engine_evaluate_now(engine_), QX_OK);

    // Deallocate all leaves that are still active
    char id1[37]{}, id3[37]{};
    ASSERT_EQ(qx_leaf_id(l1, id1), QX_OK);
    ASSERT_EQ(qx_leaf_id(l3, id3), QX_OK);

    // Leaves may have been evicted by pressure — ignore errors on dealloc
    qx_engine_deallocate(engine_, id1);
    qx_engine_deallocate(engine_, id2);
    qx_engine_deallocate(engine_, id3);
}

TEST_F(QXFullCycleTest, EvalAfterLargeBatchAllocSucceeds) {
    static constexpr int kCount = 10;
    char ids[kCount][37]{};
    QXLeafHandle leaves[kCount]{};

    const char *segs[kCount] = {
        "QLM-CORE", "QLM-UI",   "QLM-DATA", "QLM-IMG", "QLM-NET",
        "QLM-AI",   "QLM-SEC",  "QLM-LOG",  "QLM-TEMP","QLM-CORE"
    };

    int allocated = 0;
    for (int i = 0; i < kCount; ++i) {
        char label[64]{};
        std::snprintf(label, sizeof(label), "batch.leaf.%02d", i);
        QXResult rc = qx_engine_allocate(
            engine_, label, segs[i],
            QX_LEAF_CLASS_A, 4u * 1024u, &leaves[i]);
        if (rc == QX_OK) {
            qx_leaf_id(leaves[i], ids[allocated]);
            ++allocated;
        }
    }

    EXPECT_GT(allocated, 0);
    EXPECT_EQ(qx_engine_evaluate_now(engine_), QX_OK);

    for (int i = 0; i < allocated; ++i)
        qx_engine_deallocate(engine_, ids[i]);
}

TEST_F(QXFullCycleTest, IntegrityReportIsPendingBeforeEvaluation) {
    QXIntegrityReport report{};
    ASSERT_EQ(qx_engine_integrity_report(engine_, &report), QX_OK);
    EXPECT_EQ(report.status, QX_INTEGRITY_STATUS_PENDING);
    EXPECT_EQ(report.stage_passed[QX_INTEGRITY_ACCEPTANCE], QX_TRUE);
    EXPECT_EQ(report.stage_passed[QX_INTEGRITY_IMPLEMENTATION], QX_TRUE);
    EXPECT_EQ(report.stage_passed[QX_INTEGRITY_PATIENCE], QX_FALSE);
}

TEST_F(QXFullCycleTest, IntegrityStatusNullOutputReturnsError) {
    EXPECT_EQ(qx_engine_integrity_status(engine_, nullptr), QX_ERR_NULL_HANDLE);
}
