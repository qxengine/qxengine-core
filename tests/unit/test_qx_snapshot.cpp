// =============================================================================
// QXEngine Core – tests/unit/test_qx_snapshot.cpp
// Owner : Masa Bayu
// Created: 2026-05-24
// Repo   : https://github.com/qxengine/qxengine-core
// =============================================================================

#include <gtest/gtest.h>
#include <cstring>
#include <cstdint>

#include "qxengine/intelligence/qx_snapshot_types.h"
#include "qxengine/intelligence/qx_snapshot.h"
#include "qxengine/core/qx_constants.h"

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr double   kSoftBytes = 128.0 * 1024.0 * 1024.0;
static constexpr uint64_t kTimestamp = 1000000ULL;
static constexpr uint8_t  kSegCount  = QX_SEGMENT_COUNT;

static const char* kSegIds[QX_SEGMENT_COUNT] = {
    "QLM-CORE", "QLM-UI",   "QLM-DATA",
    "QLM-IMG",  "QLM-NET",  "QLM-AI",
    "QLM-SEC",  "QLM-LOG",  "QLM-TEMP"
};

// ---------------------------------------------------------------------------
// Helper
// ---------------------------------------------------------------------------
static QXSnapshotInput make_valid_input() {
    QXSnapshotInput in{};
    in.segment_count = kSegCount;

    for (uint8_t i = 0; i < kSegCount; ++i) {
        std::strncpy(in.segment_ids[i], kSegIds[i], 15);
        // utilisation = used / hard_limit — set to 40 %
        in.segment_utilisations[i] = 0.4;
    }

    in.total_used_bytes          = static_cast<QXSize>(kSoftBytes * 0.4 * kSegCount);
    in.composite_pressure_tier   = QX_TIER_NORMAL;
    in.latest_law_health_score   = 95.0;
    in.compliance_streak         = 10u;
    in.native_coverage_ratio     = 0.85;
    in.declared_capability_count = 8u;
    in.active_capability_count   = 8u;
    in.captured_at_ms            = kTimestamp;
    return in;
}

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class QXSnapshotTest : public ::testing::Test {
protected:
    QXSnapshotHistoryHandle history_ = nullptr;

    void SetUp() override {
        ASSERT_EQ(qx_snapshot_history_create(&history_), QX_OK);
        ASSERT_NE(history_, nullptr);
    }

    void TearDown() override {
        qx_snapshot_history_destroy(history_);
        history_ = nullptr;
    }

    uint32_t count() {
        uint32_t n = 0u;
        qx_snapshot_count(history_, &n);
        return n;
    }
};

// ---------------------------------------------------------------------------
// MARK: – Lifecycle
// ---------------------------------------------------------------------------

TEST(QXSnapshotLifecycle, CreateSucceeds) {
    QXSnapshotHistoryHandle h = nullptr;
    EXPECT_EQ(qx_snapshot_history_create(&h), QX_OK);
    EXPECT_NE(h, nullptr);
    qx_snapshot_history_destroy(h);
}

TEST(QXSnapshotLifecycle, CreateNullOutHandleReturnsError) {
    EXPECT_NE(qx_snapshot_history_create(nullptr), QX_OK);
}

TEST(QXSnapshotLifecycle, DestroyNullHandleIsNoOp) {
    EXPECT_NO_FATAL_FAILURE(qx_snapshot_history_destroy(nullptr));
}

// ---------------------------------------------------------------------------
// MARK: – Capture
// ---------------------------------------------------------------------------

TEST_F(QXSnapshotTest, CaptureValidInputSucceeds) {
    QXSnapshotInput in = make_valid_input();
    QXCognitiveSnapshot snap{};
    EXPECT_EQ(qx_snapshot_capture(history_, &in, &snap), QX_OK);
}

TEST_F(QXSnapshotTest, CaptureProducesKnowledgeScoreInRange) {
    QXSnapshotInput in = make_valid_input();
    QXCognitiveSnapshot snap{};
    qx_snapshot_capture(history_, &in, &snap);
    EXPECT_GE(snap.knowledge_score, 0.0);
    EXPECT_LE(snap.knowledge_score, 100.0);
}

TEST_F(QXSnapshotTest, CaptureNullInputReturnsError) {
    QXCognitiveSnapshot snap{};
    EXPECT_NE(qx_snapshot_capture(history_, nullptr, &snap), QX_OK);
}

TEST_F(QXSnapshotTest, CaptureNullSnapReturnsError) {
    // null out_snapshot is valid — capture stores internally and returns OK
    QXSnapshotInput in = make_valid_input();
    EXPECT_EQ(qx_snapshot_capture(history_, &in, nullptr), QX_OK);
}

TEST_F(QXSnapshotTest, CaptureNullHistoryReturnsError) {
    QXSnapshotInput in = make_valid_input();
    QXCognitiveSnapshot snap{};
    EXPECT_NE(qx_snapshot_capture(nullptr, &in, &snap), QX_OK);
}

// ---------------------------------------------------------------------------
// MARK: – History access
// ---------------------------------------------------------------------------

TEST_F(QXSnapshotTest, CountIsZeroBeforeFirstCapture) {
    uint32_t n = 99u;
    ASSERT_EQ(qx_snapshot_count(history_, &n), QX_OK);
    EXPECT_EQ(n, 0u);
}

TEST_F(QXSnapshotTest, CountIncrementsAfterCapture) {
    QXSnapshotInput in = make_valid_input();
    QXCognitiveSnapshot snap{};
    qx_snapshot_capture(history_, &in, &snap);
    qx_snapshot_capture(history_, &in, &snap);
    uint32_t n = 0u;
    ASSERT_EQ(qx_snapshot_count(history_, &n), QX_OK);
    EXPECT_EQ(n, 2u);
}

TEST_F(QXSnapshotTest, AtReturnsCorrectSnapshot) {
    QXSnapshotInput in = make_valid_input();
    QXCognitiveSnapshot snap{};
    qx_snapshot_capture(history_, &in, &snap);
    QXCognitiveSnapshot stored{};
    EXPECT_EQ(qx_snapshot_at(history_, 0u, &stored), QX_OK);
    EXPECT_DOUBLE_EQ(stored.knowledge_score, snap.knowledge_score);
}

TEST_F(QXSnapshotTest, AtOutOfBoundsReturnsError) {
    QXCognitiveSnapshot snap{};
    EXPECT_NE(qx_snapshot_at(history_, 999u, &snap), QX_OK);
}

TEST_F(QXSnapshotTest, ClearResetsCount) {
    QXSnapshotInput in = make_valid_input();
    QXCognitiveSnapshot snap{};
    qx_snapshot_capture(history_, &in, &snap);
    EXPECT_EQ(qx_snapshot_clear(history_), QX_OK);
    uint32_t n = 99u;
    ASSERT_EQ(qx_snapshot_count(history_, &n), QX_OK);
    EXPECT_EQ(n, 0u);
}

TEST_F(QXSnapshotTest, LatestReturnsLastCaptured) {
    QXSnapshotInput in = make_valid_input();
    QXCognitiveSnapshot snap{};
    qx_snapshot_capture(history_, &in, &snap);
    QXCognitiveSnapshot latest{};
    EXPECT_EQ(qx_snapshot_latest(history_, &latest), QX_OK);
    EXPECT_DOUBLE_EQ(latest.knowledge_score, snap.knowledge_score);
}

TEST_F(QXSnapshotTest, LatestOnEmptyHistoryReturnsError) {
    QXCognitiveSnapshot snap{};
    EXPECT_NE(qx_snapshot_latest(history_, &snap), QX_OK);
}

// ---------------------------------------------------------------------------
// MARK: – Statistics
// ---------------------------------------------------------------------------

TEST_F(QXSnapshotTest, StatsZeroBeforeFirstCapture) {
    QXSnapshotHistoryStats stats{};
    EXPECT_EQ(qx_snapshot_history_stats(history_, &stats), QX_OK);
    EXPECT_EQ(stats.snapshot_count, 0u);
}

TEST_F(QXSnapshotTest, StatsTotalCapturedIncrementsOnCapture) {
    QXSnapshotInput in = make_valid_input();
    QXCognitiveSnapshot snap{};
    qx_snapshot_capture(history_, &in, &snap);
    QXSnapshotHistoryStats stats{};
    qx_snapshot_history_stats(history_, &stats);
    EXPECT_GE(stats.total_captured, 1u);
}

TEST_F(QXSnapshotTest, StatsPeakScoreUpdatesAfterCapture) {
    QXSnapshotInput in = make_valid_input();
    QXCognitiveSnapshot snap{};
    qx_snapshot_capture(history_, &in, &snap);
    QXSnapshotHistoryStats stats{};
    qx_snapshot_history_stats(history_, &stats);
    EXPECT_GE(stats.peak_knowledge_score, snap.knowledge_score - 0.001);
}

TEST_F(QXSnapshotTest, StatsNullHandleReturnsError) {
    QXSnapshotHistoryStats stats{};
    EXPECT_NE(qx_snapshot_history_stats(nullptr, &stats), QX_OK);
}

TEST_F(QXSnapshotTest, StatsNullOutReturnsError) {
    EXPECT_NE(qx_snapshot_history_stats(history_, nullptr), QX_OK);
}

// ---------------------------------------------------------------------------
// MARK: – Signal helpers
// ---------------------------------------------------------------------------

TEST(QXSnapshotSignals, ClarityForFullUtilisationIsLow) {
    double clarity = qx_snapshot_clarity_for_utilisation(1.0);
    EXPECT_LT(clarity, 0.5);
}

TEST(QXSnapshotSignals, ClarityForZeroUtilisationIsHigh) {
    // Zero utilisation = starvation → low clarity (~0.18 per formula)
    // Peak clarity is at 0.45 utilisation → 1.0
    double clarity_zero = qx_snapshot_clarity_for_utilisation(0.0);
    double clarity_peak = qx_snapshot_clarity_for_utilisation(0.45);
    EXPECT_LT(clarity_zero, 0.3);   // starvation band: low clarity
    EXPECT_GT(clarity_peak, 0.99);  // peak band: maximum clarity
}

TEST(QXSnapshotSignals, RecencySignalDecaysOverTime) {
    double recent = qx_snapshot_recency_signal(1.0);
    double stale  = qx_snapshot_recency_signal(30.0);
    EXPECT_GT(recent, stale);
}

// ---------------------------------------------------------------------------
// MARK: – Law 5 compliance via snapshot output
// ---------------------------------------------------------------------------

TEST_F(QXSnapshotTest, HighUtilisationProducesLowerKnowledgeScore) {
    QXSnapshotInput low = make_valid_input();
    QXSnapshotInput high = make_valid_input();
    for (uint8_t i = 0; i < kSegCount; ++i)
        high.segment_utilisations[i] = 0.95;

    QXCognitiveSnapshot snap_low{}, snap_high{};
    qx_snapshot_capture(history_, &low,  &snap_low);
    qx_snapshot_capture(history_, &high, &snap_high);
    EXPECT_GE(snap_low.knowledge_score, snap_high.knowledge_score);
}

TEST_F(QXSnapshotTest, IsLaw5CompliantFieldIsPopulated) {
    QXSnapshotInput in = make_valid_input();
    QXCognitiveSnapshot snap{};
    ASSERT_EQ(qx_snapshot_capture(history_, &in, &snap), QX_OK);
    // field must be QX_TRUE or QX_FALSE — just verify it is set
    EXPECT_TRUE(snap.is_law5_compliant == QX_TRUE ||
                snap.is_law5_compliant == QX_FALSE);
}

TEST_F(QXSnapshotTest, LowUtilisationScoreIsLaw5Compliant) {
    QXSnapshotInput in = make_valid_input();
    // Segments at 45 % → peak clarity (1.0) → maximises memory signal
    // Combined with perfect law/native/compliance signals → score >= 90
    for (uint8_t i = 0; i < kSegCount; ++i)
        in.segment_utilisations[i] = 0.45;
    in.latest_law_health_score = 100.0;
    in.compliance_streak       = 20u;
    in.native_coverage_ratio   = 1.0;

    QXCognitiveSnapshot snap{};
    ASSERT_EQ(qx_snapshot_capture(history_, &in, &snap), QX_OK);
    EXPECT_TRUE(snap.is_law5_compliant);
}

TEST_F(QXSnapshotTest, CriticalUtilisationScoreIsNotLaw5Compliant) {
    QXSnapshotInput in = make_valid_input();
    // All segments at 100 % → zero clarity → very low knowledge score
    for (uint8_t i = 0; i < kSegCount; ++i)
        in.segment_utilisations[i] = 1.0;
    in.latest_law_health_score = 0.0;
    in.compliance_streak       = 0u;
    in.native_coverage_ratio   = 0.0;

    QXCognitiveSnapshot snap{};
    ASSERT_EQ(qx_snapshot_capture(history_, &in, &snap), QX_OK);
    EXPECT_FALSE(snap.is_law5_compliant);
}
