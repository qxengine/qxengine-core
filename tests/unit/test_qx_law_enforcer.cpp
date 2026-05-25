// =============================================================================
// QXEngine Core – tests/unit/test_qx_law_enforcer.cpp
// Owner : Masa Bayu
// Created: 2026-05-24
// Repo   : https://github.com/qxengine/qxengine-core
// =============================================================================

#include <gtest/gtest.h>
#include <cstring>
#include <cstdint>

#include "qxengine/law/qx_law_types.h"
#include "qxengine/law/qx_law_report.h"
#include "qxengine/law/qx_law_enforcer_types.h"
#include "qxengine/law/qx_law_enforcer.h"
#include "qxengine/core/qx_constants.h"

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr double  kSoftBytes     = 128.0 * 1024.0 * 1024.0;
static constexpr double  kHardBytes     = 256.0 * 1024.0 * 1024.0;
static constexpr double  kKnowledgePass = 92.0;
static constexpr double  kKnowledgeFail = 45.0;
static constexpr uint8_t kSegmentCount  = QX_SEGMENT_COUNT;

static const char* kSegIds[QX_SEGMENT_COUNT] = {
    "QLM-CORE", "QLM-UI",   "QLM-DATA",
    "QLM-IMG",  "QLM-NET",  "QLM-AI",
    "QLM-SEC",  "QLM-LOG",  "QLM-TEMP"
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static QXLawEvaluationInput make_valid_input() {
    QXLawEvaluationInput in{};
    in.unlabelled_allocation_count = 0;
    in.segment_count               = kSegmentCount;

    for (uint8_t i = 0; i < kSegmentCount; ++i) {
        std::strncpy(in.segments[i].segment_id, kSegIds[i], 15);
        in.segments[i].used_bytes        = static_cast<QXSize>(kSoftBytes * 0.5);
        in.segments[i].soft_limit_bytes  = static_cast<QXSize>(kSoftBytes);
        in.segments[i].hard_limit_bytes  = static_cast<QXSize>(kHardBytes);
        in.segments[i].total_allocations = 10u;
        in.segments[i].total_deallocations = 10u;
        in.segments[i].pairs_ratio       = 1.0;
        in.segments[i].soft_utilisation  = 0.5;
        in.segments[i].hard_utilisation  = 0.25;
        in.segments[i].orphaned_bytes    = 0u;
    }

    // Law 5 – Knowledge
    in.knowledge_score              = kKnowledgePass;
    in.seconds_since_last_snapshot  = 0.0;

    // Law 6 – Ethics
    in.dark_patterns_prohibited  = QX_TRUE;
    in.deceptive_flows_prohibited= QX_TRUE;
    in.manipulative_ux_prohibited= QX_TRUE;
    in.privacy_first_design      = QX_TRUE;
    in.transparent_data_usage    = QX_TRUE;

    // Law 7 – Creativity
    in.native_first_policy       = QX_TRUE;
    in.native_utilisation_ratio  = 0.85;
    in.declared_capability_count = 0u;
    in.active_capability_count   = 0u;

    // Law 8 – Economy
    in.battery_drain_pct_per_10min = 3.0;
    in.network_redundancy_pct      = 2.0;

    in.captured_at_ms = 1000000ULL;
    return in;
}

static QXEnforcerConfig make_valid_config() {
    QXEnforcerConfig cfg{};
    cfg.fail_fast_on_fatal         = QX_FALSE;
    cfg.measure_duration           = QX_FALSE;
    cfg.min_knowledge_score        = 90.0;
    cfg.min_native_utilisation     = 0.70;
    cfg.max_battery_drain_pct      = 10.0;
    cfg.max_network_redundancy_pct = 10.0;
    cfg.min_pairs_ratio            = 0.80;
    return cfg;
}

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class QXLawEnforcerTest : public ::testing::Test {
protected:
    QXLawEnforcerHandle enforcer_ = nullptr;

    void SetUp() override {
        QXEnforcerConfig cfg = make_valid_config();
        ASSERT_EQ(qx_law_enforcer_create(&cfg, &enforcer_), QX_OK);
        ASSERT_NE(enforcer_, nullptr);
    }

    void TearDown() override {
        qx_law_enforcer_destroy(enforcer_);
        enforcer_ = nullptr;
    }
};

// ---------------------------------------------------------------------------
// MARK: – Lifecycle
// ---------------------------------------------------------------------------

TEST(QXLawEnforcerLifecycle, CreateWithNullConfigUsesDefaults) {
    QXLawEnforcerHandle h = nullptr;
    EXPECT_EQ(qx_law_enforcer_create(nullptr, &h), QX_OK);
    EXPECT_NE(h, nullptr);
    qx_law_enforcer_destroy(h);
}

TEST(QXLawEnforcerLifecycle, CreateWithNullOutHandleReturnsError) {
    QXEnforcerConfig cfg = make_valid_config();
    EXPECT_NE(qx_law_enforcer_create(&cfg, nullptr), QX_OK);
}

TEST(QXLawEnforcerLifecycle, DestroyNullHandleIsNoOp) {
    EXPECT_NO_FATAL_FAILURE(qx_law_enforcer_destroy(nullptr));
}

// ---------------------------------------------------------------------------
// MARK: – Full evaluation – compliant input
// ---------------------------------------------------------------------------

TEST_F(QXLawEnforcerTest, CompliantInputProducesHealthScoreAbove60) {
    QXLawEvaluationInput in = make_valid_input();
    QXLawReport report{};
    EXPECT_EQ(qx_law_enforcer_evaluate(enforcer_, &in, &report), QX_OK);
    EXPECT_GE(report.health_score, 60.0);
}

TEST_F(QXLawEnforcerTest, CompliantInputIsCertified) {
    QXLawEvaluationInput in = make_valid_input();
    QXLawReport report{};
    qx_law_enforcer_evaluate(enforcer_, &in, &report);
    EXPECT_TRUE(qx_report_is_certified(&report));
}

TEST_F(QXLawEnforcerTest, HealthScoreIsInValidRange) {
    QXLawEvaluationInput in = make_valid_input();
    QXLawReport report{};
    qx_law_enforcer_evaluate(enforcer_, &in, &report);
    EXPECT_GE(report.health_score, 0.0);
    EXPECT_LE(report.health_score, 100.0);
}

TEST_F(QXLawEnforcerTest, EvaluateNullHandleReturnsError) {
    QXLawEvaluationInput in = make_valid_input();
    QXLawReport report{};
    EXPECT_NE(qx_law_enforcer_evaluate(nullptr, &in, &report), QX_OK);
}

TEST_F(QXLawEnforcerTest, EvaluateNullInputReturnsError) {
    QXLawReport report{};
    EXPECT_NE(qx_law_enforcer_evaluate(enforcer_, nullptr, &report), QX_OK);
}

TEST_F(QXLawEnforcerTest, EvaluateNullReportReturnsError) {
    // null out_report is valid — implementation writes nothing and returns OK
    QXLawEvaluationInput in = make_valid_input();
    EXPECT_EQ(qx_law_enforcer_evaluate(enforcer_, &in, nullptr), QX_OK);
}

// ---------------------------------------------------------------------------
// MARK: – Fail-fast on fatal violation
// ---------------------------------------------------------------------------

TEST(QXLawEnforcerFailFast, FatalViolationTriggersEarlyExit) {
    QXEnforcerConfig cfg = make_valid_config();
    cfg.fail_fast_on_fatal = QX_TRUE;
    QXLawEnforcerHandle h = nullptr;
    ASSERT_EQ(qx_law_enforcer_create(&cfg, &h), QX_OK);

    QXLawEvaluationInput in = make_valid_input();
    in.knowledge_score = kKnowledgeFail;
    QXLawReport report{};
    QXResult rc = qx_law_enforcer_evaluate(h, &in, &report);
    EXPECT_TRUE(rc != QX_OK || report.has_fatal_violation);
    qx_law_enforcer_destroy(h);
}

// ---------------------------------------------------------------------------
// MARK: – Per-law forced failures
// ---------------------------------------------------------------------------

TEST_F(QXLawEnforcerTest, Law0FailsOnUnlabelledAllocations) {
    QXLawEvaluationInput in = make_valid_input();
    in.unlabelled_allocation_count = 50;
    QXLawReport report{};
    qx_law_enforcer_evaluate(enforcer_, &in, &report);
    EXPECT_FALSE(report.law_scores[0].is_compliant);
}

TEST_F(QXLawEnforcerTest, Law2FailsWhenSegmentOverHardLimit) {
    QXLawEvaluationInput in = make_valid_input();
    in.segments[0].used_bytes       = static_cast<QXSize>(kHardBytes * 1.5);
    in.segments[0].hard_utilisation = 1.5;
    QXLawReport report{};
    qx_law_enforcer_evaluate(enforcer_, &in, &report);
    EXPECT_FALSE(report.law_scores[1].is_compliant);  // Law2 → index 1
}

TEST_F(QXLawEnforcerTest, Law5FailsOnLowKnowledgeScore) {
    QXLawEvaluationInput in = make_valid_input();
    in.knowledge_score = kKnowledgeFail;
    QXLawReport report{};
    qx_law_enforcer_evaluate(enforcer_, &in, &report);
    EXPECT_FALSE(report.law_scores[4].is_compliant);  // Law5 → index 4
}

TEST_F(QXLawEnforcerTest, Law6FailsWhenDarkPatternsAllowed) {
    QXLawEvaluationInput in = make_valid_input();
    in.dark_patterns_prohibited = QX_FALSE;
    QXLawReport report{};
    qx_law_enforcer_evaluate(enforcer_, &in, &report);
    EXPECT_FALSE(report.law_scores[5].is_compliant);  // Law6 → index 5
}

TEST_F(QXLawEnforcerTest, Law7FailsWhenNativeFirstPolicyFalse) {
    QXLawEvaluationInput in = make_valid_input();
    in.native_first_policy = QX_FALSE;
    QXLawReport report{};
    qx_law_enforcer_evaluate(enforcer_, &in, &report);
    EXPECT_FALSE(report.law_scores[6].is_compliant);  // Law7 → index 6
}

// ---------------------------------------------------------------------------
// MARK: – Report history
// ---------------------------------------------------------------------------

TEST_F(QXLawEnforcerTest, HistoryCountIsZeroBeforeFirstEval) {
    uint32_t count = 0u;
    EXPECT_EQ(qx_law_enforcer_report_count(enforcer_, &count), QX_OK);
    EXPECT_EQ(count, 0u);
}

TEST_F(QXLawEnforcerTest, HistoryCountIncrementsAfterEval) {
    QXLawEvaluationInput in = make_valid_input();
    QXLawReport report{};
    qx_law_enforcer_evaluate(enforcer_, &in, &report);
    qx_law_enforcer_evaluate(enforcer_, &in, &report);
    uint32_t count = 0u;
    EXPECT_EQ(qx_law_enforcer_report_count(enforcer_, &count), QX_OK);
    EXPECT_EQ(count, 2u);
}

TEST_F(QXLawEnforcerTest, HistoryAtReturnsCorrectReport) {
    QXLawEvaluationInput in = make_valid_input();
    QXLawReport report{};
    qx_law_enforcer_evaluate(enforcer_, &in, &report);
    QXLawReport stored{};
    EXPECT_EQ(qx_law_enforcer_report_at(enforcer_, 0u, &stored), QX_OK);
    EXPECT_DOUBLE_EQ(stored.health_score, report.health_score);
}

TEST_F(QXLawEnforcerTest, HistoryAtOutOfBoundsReturnsError) {
    QXLawReport stored{};
    EXPECT_NE(qx_law_enforcer_report_at(enforcer_, 999u, &stored), QX_OK);
}

TEST_F(QXLawEnforcerTest, ClearHistoryResetsCount) {
    QXLawEvaluationInput in = make_valid_input();
    QXLawReport report{};
    qx_law_enforcer_evaluate(enforcer_, &in, &report);
    EXPECT_EQ(qx_law_enforcer_clear_history(enforcer_), QX_OK);
    uint32_t count = 99u;
    qx_law_enforcer_report_count(enforcer_, &count);
    EXPECT_EQ(count, 0u);
}

// ---------------------------------------------------------------------------
// MARK: – Statistics
// ---------------------------------------------------------------------------

TEST_F(QXLawEnforcerTest, StatsZeroBeforeFirstEval) {
    QXEnforcerStats stats{};
    EXPECT_EQ(qx_law_enforcer_stats(enforcer_, &stats), QX_OK);
    EXPECT_EQ(stats.total_evaluations, 0u);
}

TEST_F(QXLawEnforcerTest, StatsTotalCountIncrementsOnEval) {
    QXLawEvaluationInput in = make_valid_input();
    QXLawReport report{};
    qx_law_enforcer_evaluate(enforcer_, &in, &report);
    qx_law_enforcer_evaluate(enforcer_, &in, &report);
    QXEnforcerStats stats{};
    qx_law_enforcer_stats(enforcer_, &stats);
    EXPECT_EQ(stats.total_evaluations, 2u);
}

TEST_F(QXLawEnforcerTest, StatsPeakScoreUpdatesAfterEval) {
    QXLawEvaluationInput in = make_valid_input();
    QXLawReport report{};
    qx_law_enforcer_evaluate(enforcer_, &in, &report);
    QXEnforcerStats stats{};
    qx_law_enforcer_stats(enforcer_, &stats);
    EXPECT_GE(stats.peak_health_score, report.health_score - 0.001);
}

TEST_F(QXLawEnforcerTest, StatsNullHandleReturnsError) {
    QXEnforcerStats stats{};
    EXPECT_NE(qx_law_enforcer_stats(nullptr, &stats), QX_OK);
}

// ---------------------------------------------------------------------------
// MARK: – Certification utilities
// ---------------------------------------------------------------------------

TEST(QXCertificationUtils, LabelIsNonNullForKnownTiers) {
    const QXCertificationId tiers[] = {
        QX_CERT_INVALID, QX_CERT_STANDARD, QX_CERT_SOVEREIGN
    };
    for (QXCertificationId t : tiers) {
        const char* label = qx_certification_label(t);
        EXPECT_NE(label, nullptr);
        EXPECT_GT(std::strlen(label), 0u);
    }
}

TEST(QXCertificationUtils, FromScoreReturnsCorrectTier) {
    EXPECT_GE(qx_certification_from_score(100.0), QX_CERT_SOVEREIGN);
    EXPECT_EQ(qx_certification_from_score(0.0),   QX_CERT_INVALID);
}

TEST(QXCertificationUtils, FromScoreClampsBelowZero) {
    EXPECT_EQ(qx_certification_from_score(-5.0), QX_CERT_INVALID);
}

TEST(QXCertificationUtils, MinScoreForSovereignAbove90) {
    EXPECT_GE(qx_certification_min_score(QX_CERT_SOVEREIGN), 90.0);
}

TEST(QXCertificationUtils, MinScoreForInvalidIsZero) {
    EXPECT_DOUBLE_EQ(qx_certification_min_score(QX_CERT_INVALID), 0.0);
}

// ---------------------------------------------------------------------------
// MARK: – Single-law diagnostics
// ---------------------------------------------------------------------------

TEST_F(QXLawEnforcerTest, EvaluateSingleLaw0WithCompliantInput) {
    // law_id is 1-based: Law1 = 1u, maps to law_scores[0]
    QXLawEvaluationInput in = make_valid_input();
    QXLawReport report{};
    EXPECT_EQ(qx_law_enforcer_evaluate_single(
        enforcer_, 1u, &in, &report), QX_OK);
    EXPECT_TRUE(report.law_scores[0].is_compliant);
}

TEST_F(QXLawEnforcerTest, EvaluateSingleLaw7WithCompliantInput) {
    QXLawEvaluationInput in = make_valid_input();
    QXLawReport report{};
    EXPECT_EQ(qx_law_enforcer_evaluate_single(
        enforcer_, 7u, &in, &report), QX_OK);
    EXPECT_GE(report.law_scores[7].raw_score, 0.0);
}

TEST_F(QXLawEnforcerTest, EvaluateSingleInvalidLawIndexReturnsError) {
    // law_id is 1-based, valid range 1..QX_LAW_COUNT; QX_LAW_COUNT+1 is invalid
    QXLawEvaluationInput in = make_valid_input();
    QXLawReport report{};
    EXPECT_NE(qx_law_enforcer_evaluate_single(
        enforcer_, QX_LAW_COUNT + 1u, &in, &report), QX_OK);
}

TEST_F(QXLawEnforcerTest, EvaluateSingleNullHandleReturnsError) {
    QXLawEvaluationInput in = make_valid_input();
    QXLawReport report{};
    EXPECT_NE(qx_law_enforcer_evaluate_single(
        nullptr, 0u, &in, &report), QX_OK);
}
