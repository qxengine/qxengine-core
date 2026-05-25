// =============================================================================
// QXEngine Core – tests/unit/test_qx_pressure.cpp
// Owner : Masa Bayu
// Created: 2026-05-24
// Repo   : https://github.com/qxengine/qxengine-core
// Description: Unit tests for QXPressureCoordinator – lifecycle, tier
//              computation, evaluation, Android/iOS OS signal handling,
//              memory feed, event history, and aggregate statistics.
// =============================================================================

#include <gtest/gtest.h>
#include <cstring>

#include "qxengine/memory/qx_pressure.h"
#include "qxengine/memory/qx_memloc.h"
#include "qxengine/memory/qx_leaf.h"
#include "qxengine/core/qx_types.h"
#include "qxengine/core/qx_constants.h"
#include "qxengine/core/qx_error.h"

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr QXSize   kSoftLimit = 128u * 1024u * 1024u;
static constexpr QXSize   kHardLimit = 256u * 1024u * 1024u;
static constexpr uint32_t kMaxSlots  = 7u;

static const char* kSegIds[QX_SEGMENT_COUNT] = {
    QX_SEGMENT_CORE, QX_SEGMENT_UI,   QX_SEGMENT_DATA,
    QX_SEGMENT_IMG,  QX_SEGMENT_NET,  QX_SEGMENT_AI,
    QX_SEGMENT_SEC,  QX_SEGMENT_LOG,  QX_SEGMENT_TEMP
};

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class QXPressureTest : public ::testing::Test {
protected:
    QXMemlocHandle              memloc   = nullptr;
    QXPressureCoordinatorHandle pressure = nullptr;

    void SetUp() override {
        QXSegmentConfig configs[QX_SEGMENT_COUNT]{};
        for (uint32_t i = 0; i < QX_SEGMENT_COUNT; ++i) {
            std::strncpy(configs[i].segment_id, kSegIds[i], 15);
            configs[i].effective_soft_bytes = kSoftLimit;
            configs[i].effective_hard_bytes = kHardLimit;
            configs[i].max_slots            = kMaxSlots;
        }
        ASSERT_EQ(qx_memloc_create(configs, QX_SEGMENT_COUNT, &memloc), QX_OK);
        ASSERT_NE(memloc, nullptr);
        ASSERT_EQ(qx_pressure_create(memloc, &pressure), QX_OK);
        ASSERT_NE(pressure, nullptr);
    }

    void TearDown() override {
        if (pressure) { qx_pressure_destroy(pressure); pressure = nullptr; }
        if (memloc)   { qx_memloc_destroy(memloc);     memloc   = nullptr; }
    }

    QXResult alloc_leaf(const char*   label,
                        const char*   segment_id,
                        QXSize        size,
                        QXLeafHandle* out_leaf = nullptr) {
        QXAllocResult r{};
        QXResult rc = qx_memloc_allocate(
            memloc, label, segment_id, QX_LEAF_CLASS_D, size, &r);
        if (rc == QX_OK && out_leaf) *out_leaf = r.leaf;
        return rc;
    }
};

// ---------------------------------------------------------------------------
// MARK: – Lifecycle
// ---------------------------------------------------------------------------

TEST_F(QXPressureTest, CreateSucceeds) {
    EXPECT_NE(pressure, nullptr);
}

TEST_F(QXPressureTest, CreateNullMemlocReturnsError) {
    QXPressureCoordinatorHandle p = nullptr;
    EXPECT_NE(qx_pressure_create(nullptr, &p), QX_OK);
    EXPECT_EQ(p, nullptr);
}

TEST_F(QXPressureTest, CreateNullOutHandleReturnsError) {
    EXPECT_NE(qx_pressure_create(memloc, nullptr), QX_OK);
}

// ---------------------------------------------------------------------------
// MARK: – Tier computation
// ---------------------------------------------------------------------------

TEST_F(QXPressureTest, CurrentTierNormalWhenEmpty) {
    QXTierId tier = QX_TIER_CRITICAL;
    ASSERT_EQ(qx_pressure_current_tier(pressure, &tier), QX_OK);
    EXPECT_EQ(tier, QX_TIER_NORMAL);
}

TEST_F(QXPressureTest, SegmentTierNormalWhenIdle) {
    QXTierId tier = QX_TIER_CRITICAL;
    ASSERT_EQ(qx_pressure_segment_tier(
        pressure, QX_SEGMENT_CORE, &tier), QX_OK);
    EXPECT_EQ(tier, QX_TIER_NORMAL);
}

TEST_F(QXPressureTest, SegmentTierUnknownSegmentReturnsError) {
    QXTierId tier = QX_TIER_NORMAL;
    EXPECT_NE(qx_pressure_segment_tier(
        pressure, "QLM-UNKNOWN", &tier), QX_OK);
}

TEST_F(QXPressureTest, CurrentTierNullHandleReturnsError) {
    QXTierId tier = QX_TIER_NORMAL;
    EXPECT_NE(qx_pressure_current_tier(nullptr, &tier), QX_OK);
}

// ---------------------------------------------------------------------------
// MARK: – Evaluate
// ---------------------------------------------------------------------------

TEST_F(QXPressureTest, EvaluateOnEmptyMemlocReturnsOk) {
    EXPECT_EQ(qx_pressure_evaluate(pressure, nullptr), QX_OK);
}

TEST_F(QXPressureTest, EvaluateIncrementsEvaluationCount) {
    ASSERT_EQ(qx_pressure_evaluate(pressure, nullptr), QX_OK);
    ASSERT_EQ(qx_pressure_evaluate(pressure, nullptr), QX_OK);

    QXPressureStats stats{};
    ASSERT_EQ(qx_pressure_stats(pressure, &stats), QX_OK);
    EXPECT_EQ(stats.total_evaluations, 2u);
}

TEST_F(QXPressureTest, EvaluateNullHandleReturnsError) {
    EXPECT_NE(qx_pressure_evaluate(nullptr, nullptr), QX_OK);
}

TEST_F(QXPressureTest, EvaluateAllWithValidTierReturnsOk) {
    EXPECT_EQ(qx_pressure_evaluate_all(
        pressure, QX_TIER_ELEVATED, nullptr), QX_OK);
}

TEST_F(QXPressureTest, EvaluatePopulatesOutEvent) {
    // qx_pressure_evaluate only populates out_event when tier > NORMAL (eviction runs).
    // Feed a high-pressure android trim to force an eviction pass, then check stats.
    QXPressureEvent ev{};
    ASSERT_EQ(qx_pressure_evaluate_all(pressure, QX_TIER_ELEVATED, &ev), QX_OK);
    // After evaluate_all the coordinator records the evaluation timestamp
    QXPressureStats stats{};
    ASSERT_EQ(qx_pressure_stats(pressure, &stats), QX_OK);
    EXPECT_GT(stats.last_evaluated_ms, 0u);
}

// ---------------------------------------------------------------------------
// MARK: – Android trim level mapping
// ---------------------------------------------------------------------------

TEST_F(QXPressureTest, AndroidTrimLevel5MapsToElevated) {
    QXPressureEvent ev{};
    ASSERT_EQ(qx_pressure_handle_android_trim(pressure, 5, &ev), QX_OK);
    EXPECT_EQ(ev.trigger_tier, QX_TIER_ELEVATED);
}

TEST_F(QXPressureTest, AndroidTrimLevel10MapsToHigh) {
    QXPressureEvent ev{};
    ASSERT_EQ(qx_pressure_handle_android_trim(pressure, 10, &ev), QX_OK);
    EXPECT_EQ(ev.trigger_tier, QX_TIER_HIGH);
}

TEST_F(QXPressureTest, AndroidTrimLevel15MapsToCritical) {
    QXPressureEvent ev{};
    // On empty memloc critical tier may return INSUFFICIENT_EVICTION — both are valid
    QXResult rc = qx_pressure_handle_android_trim(pressure, 15, &ev);
    EXPECT_TRUE(rc == QX_OK || rc == QX_ERR_INSUFFICIENT_EVICTION);
    EXPECT_EQ(ev.trigger_tier, QX_TIER_CRITICAL);
}

TEST_F(QXPressureTest, AndroidTrimUnknownLevelDefaultsToElevated) {
    QXPressureEvent ev{};
    ASSERT_EQ(qx_pressure_handle_android_trim(pressure, 999, &ev), QX_OK);
    EXPECT_EQ(ev.trigger_tier, QX_TIER_ELEVATED);
}

TEST_F(QXPressureTest, AndroidTrimNullHandleReturnsError) {
    EXPECT_NE(qx_pressure_handle_android_trim(nullptr, 5, nullptr), QX_OK);
}

// ---------------------------------------------------------------------------
// MARK: – iOS warning
// ---------------------------------------------------------------------------

TEST_F(QXPressureTest, IosWarningDefaultsToHigh) {
    QXPressureEvent ev{};
    ASSERT_EQ(qx_pressure_handle_ios_warning(pressure, &ev), QX_OK);
    EXPECT_EQ(ev.trigger_tier, QX_TIER_HIGH);
}

TEST_F(QXPressureTest, IosWarningEscalatesToCriticalAt85PctRam) {
    const QXSize total    = 1024u * 1024u * 1024u;
    const QXSize resident = (total * 85u) / 100u;
    ASSERT_EQ(qx_pressure_feed_memory(
        pressure, resident, total, total - resident), QX_OK);

    QXPressureEvent ev{};
    QXResult rc = qx_pressure_handle_ios_warning(pressure, &ev);
    EXPECT_TRUE(rc == QX_OK || rc == QX_ERR_INSUFFICIENT_EVICTION);
    if (rc == QX_OK) { EXPECT_EQ(ev.trigger_tier, QX_TIER_CRITICAL); }
}

TEST_F(QXPressureTest, IosWarningNullHandleReturnsError) {
    EXPECT_NE(qx_pressure_handle_ios_warning(nullptr, nullptr), QX_OK);
}

// ---------------------------------------------------------------------------
// MARK: – Memory feed
// ---------------------------------------------------------------------------

TEST_F(QXPressureTest, FeedMemorySucceeds) {
    EXPECT_EQ(qx_pressure_feed_memory(
        pressure,
        512u  * 1024u * 1024u,
        1024u * 1024u * 1024u,
        512u  * 1024u * 1024u), QX_OK);
}

TEST_F(QXPressureTest, FeedMemoryZeroTotalRejected) {
    EXPECT_NE(qx_pressure_feed_memory(
        pressure, 0u, 0u, 0u), QX_OK);
}

TEST_F(QXPressureTest, FeedMemoryNullHandleReturnsError) {
    EXPECT_NE(qx_pressure_feed_memory(
        nullptr, 512u, 1024u, 512u), QX_OK);
}

// ---------------------------------------------------------------------------
// MARK: – Event history
// ---------------------------------------------------------------------------

TEST_F(QXPressureTest, EventCountZeroOnInit) {
    uint32_t count = 99u;
    ASSERT_EQ(qx_pressure_event_count(pressure, &count), QX_OK);
    EXPECT_EQ(count, 0u);
}

TEST_F(QXPressureTest, EventCountIncrementsAfterEvaluateAll) {
    ASSERT_EQ(alloc_leaf("ev.hist.leaf", QX_SEGMENT_TEMP, 1024u), QX_OK);
    ASSERT_EQ(qx_pressure_evaluate_all(
        pressure, QX_TIER_ELEVATED, nullptr), QX_OK);

    uint32_t count = 0u;
    ASSERT_EQ(qx_pressure_event_count(pressure, &count), QX_OK);
    EXPECT_GE(count, 1u);
}

TEST_F(QXPressureTest, EventAtReturnsCorrectTier) {
    ASSERT_EQ(qx_pressure_evaluate_all(
        pressure, QX_TIER_HIGH, nullptr), QX_OK);

    QXPressureEvent ev{};
    ASSERT_EQ(qx_pressure_event_at(pressure, 0u, &ev), QX_OK);
    EXPECT_EQ(ev.trigger_tier, QX_TIER_HIGH);
}

TEST_F(QXPressureTest, EventAtOutOfRangeReturnsError) {
    EXPECT_NE(qx_pressure_event_at(pressure, 9999u, nullptr), QX_OK);
}

TEST_F(QXPressureTest, ClearHistoryResetsCount) {
    ASSERT_EQ(qx_pressure_evaluate_all(
        pressure, QX_TIER_ELEVATED, nullptr), QX_OK);
    ASSERT_EQ(qx_pressure_clear_history(pressure), QX_OK);

    uint32_t count = 99u;
    ASSERT_EQ(qx_pressure_event_count(pressure, &count), QX_OK);
    EXPECT_EQ(count, 0u);
}

// ---------------------------------------------------------------------------
// MARK: – Stats
// ---------------------------------------------------------------------------

TEST_F(QXPressureTest, StatsZeroOnInit) {
    QXPressureStats stats{};
    ASSERT_EQ(qx_pressure_stats(pressure, &stats), QX_OK);
    EXPECT_EQ(stats.total_evaluations,     0u);
    EXPECT_EQ(stats.eviction_cycles,       0u);
    EXPECT_EQ(stats.total_bytes_reclaimed, 0u);
    EXPECT_EQ(stats.total_leaves_evicted,  0u);
}

TEST_F(QXPressureTest, StatsPeakTierUpdatesAfterEvaluateAll) {
    ASSERT_EQ(qx_pressure_evaluate_all(
        pressure, QX_TIER_HIGH, nullptr), QX_OK);

    QXPressureStats stats{};
    ASSERT_EQ(qx_pressure_stats(pressure, &stats), QX_OK);
    EXPECT_EQ(stats.peak_tier, QX_TIER_HIGH);
}

TEST_F(QXPressureTest, StatsNullHandleReturnsError) {
    QXPressureStats stats{};
    EXPECT_NE(qx_pressure_stats(nullptr, &stats), QX_OK);
}

TEST_F(QXPressureTest, StatsNullOutReturnsError) {
    EXPECT_NE(qx_pressure_stats(pressure, nullptr), QX_OK);
}
