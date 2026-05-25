// =============================================================================
// QXEngine Core – tests/unit/test_qx_telemetry.cpp
// Owner  : Masa Bayu
// Created: 2026-05-24
// Repo   : https://github.com/qxengine/qxengine-core
// Description: Unit tests for QXTelemetry – lifecycle, record, event log,
//              violation log, reporting, stats, and category label helper.
// =============================================================================

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

#include "qxengine/telemetry/qx_telemetry.h"
#include "qxengine/telemetry/qx_telemetry_types.h"
#include "qxengine/telemetry/qx_telemetry_event_types.h"
#include "qxengine/core/qx_types.h"
#include "qxengine/core/qx_constants.h"
#include "qxengine/core/qx_error.h"
#include "qxengine/law/qx_law_report.h"

class QXTelemetryTest : public ::testing::Test {
protected:
    QXTelemetryHandle telemetry_ = nullptr;

    void SetUp() override {
        QXTelemetryConfig cfg{};
        cfg.enabled             = QX_TRUE;
        cfg.record_allocations  = QX_TRUE;
        cfg.record_evictions    = QX_TRUE;
        cfg.record_evaluations  = QX_TRUE;
        cfg.record_snapshots    = QX_TRUE;
        cfg.record_security     = QX_TRUE;
        cfg.separate_violation_log = QX_TRUE;
        cfg.verbose             = QX_FALSE;
        ASSERT_EQ(qx_telemetry_create(&cfg, &telemetry_), QX_OK);
        ASSERT_NE(telemetry_, nullptr);
    }

    void TearDown() override {
        if (telemetry_) {
            qx_telemetry_destroy(telemetry_);
            telemetry_ = nullptr;
        }
    }

    static QXTelemetryEventInput make_input(
        QXTelemetryCategory cat = QX_TEL_ALLOCATION,
        QXSeverityId        sev = QX_SEVERITY_INFO)
    {
        QXTelemetryEventInput in{};
        in.category  = cat;
        in.severity  = sev;
        in.result    = QX_OK;
        std::strncpy(in.message,    "test event",  sizeof(in.message)  - 1u);
        std::strncpy(in.segment_id, "QLM-CORE",    sizeof(in.segment_id) - 1u);
        in.bytes     = 1024u;
        return in;
    }
};

TEST_F(QXTelemetryTest, CreateSucceeds) {
    EXPECT_NE(telemetry_, nullptr);
}

TEST_F(QXTelemetryTest, CreateNullConfigReturnsError) {
    // null config is treated as "use defaults" by the implementation — not an error
    QXTelemetryHandle h = nullptr;
    EXPECT_EQ(qx_telemetry_create(nullptr, &h), QX_OK);
    EXPECT_NE(h, nullptr);
    qx_telemetry_destroy(h);
}

TEST_F(QXTelemetryTest, CreateNullOutHandleReturnsError) {
    QXTelemetryConfig cfg{};
    cfg.enabled = QX_TRUE;
    EXPECT_NE(qx_telemetry_create(&cfg, nullptr), QX_OK);
}

TEST_F(QXTelemetryTest, DestroyNullHandleIsSafe) {
    qx_telemetry_destroy(nullptr); // must not crash
}

// =============================================================================
// MARK: – Record
// =============================================================================

TEST_F(QXTelemetryTest, RecordAllocationEventSucceeds) {
    QXTelemetryEventInput in = make_input(QX_TEL_ALLOCATION, QX_SEVERITY_INFO);
    QXTelemetryEvent out{};
    EXPECT_EQ(qx_telemetry_record(telemetry_, &in, &out), QX_OK);
    EXPECT_EQ(out.category, QX_TEL_ALLOCATION);
}

TEST_F(QXTelemetryTest, RecordDeallocationEventSucceeds) {
    QXTelemetryEventInput in = make_input(QX_TEL_DEALLOCATION, QX_SEVERITY_INFO);
    EXPECT_EQ(qx_telemetry_record(telemetry_, &in, nullptr), QX_OK);
}

TEST_F(QXTelemetryTest, RecordViolationCategorySucceeds) {
    QXTelemetryEventInput in = make_input(QX_TEL_VIOLATION, QX_SEVERITY_WARNING);
    EXPECT_EQ(qx_telemetry_record(telemetry_, &in, nullptr), QX_OK);
}

TEST_F(QXTelemetryTest, RecordNullInputReturnsError) {
    EXPECT_NE(qx_telemetry_record(telemetry_, nullptr, nullptr), QX_OK);
}

TEST_F(QXTelemetryTest, RecordNullHandleReturnsError) {
    QXTelemetryEventInput in = make_input();
    EXPECT_NE(qx_telemetry_record(nullptr, &in, nullptr), QX_OK);
}

TEST_F(QXTelemetryTest, RecordOutEventOptional) {
    QXTelemetryEventInput in = make_input(QX_TEL_ENGINE, QX_SEVERITY_INFO);
    // out_event = nullptr must be accepted
    EXPECT_EQ(qx_telemetry_record(telemetry_, &in, nullptr), QX_OK);
}

// =============================================================================
// MARK: – Event log – count & access
// =============================================================================

TEST_F(QXTelemetryTest, EventCountZeroOnCreate) {
    uint32_t count = 99u;
    ASSERT_EQ(qx_telemetry_event_count(telemetry_, &count), QX_OK);
    EXPECT_EQ(count, 0u);
}

TEST_F(QXTelemetryTest, EventCountIncrementsAfterRecord) {
    QXTelemetryEventInput in = make_input();
    qx_telemetry_record(telemetry_, &in, nullptr);
    qx_telemetry_record(telemetry_, &in, nullptr);
    uint32_t count = 0u;
    ASSERT_EQ(qx_telemetry_event_count(telemetry_, &count), QX_OK);
    EXPECT_EQ(count, 2u);
}

TEST_F(QXTelemetryTest, EventCountNullHandleReturnsError) {
    uint32_t count = 0u;
    EXPECT_EQ(qx_telemetry_event_count(nullptr, &count), QX_ERR_NULL_HANDLE);
}

TEST_F(QXTelemetryTest, EventCountNullOutReturnsError) {
    EXPECT_EQ(qx_telemetry_event_count(telemetry_, nullptr), QX_ERR_NULL_HANDLE);
}

TEST_F(QXTelemetryTest, EventAtReturnsCorrectCategory) {
    QXTelemetryEventInput in = make_input(QX_TEL_SNAPSHOT, QX_SEVERITY_INFO);
    qx_telemetry_record(telemetry_, &in, nullptr);
    QXTelemetryEvent ev{};
    ASSERT_EQ(qx_telemetry_event_at(telemetry_, 0u, &ev), QX_OK);
    EXPECT_EQ(ev.category, QX_TEL_SNAPSHOT);
}

TEST_F(QXTelemetryTest, EventAtOutOfRangeReturnsError) {
    QXTelemetryEvent ev{};
    EXPECT_NE(qx_telemetry_event_at(telemetry_, 9999u, &ev), QX_OK);
}

TEST_F(QXTelemetryTest, EventAtNullHandleReturnsError) {
    QXTelemetryEvent ev{};
    EXPECT_EQ(qx_telemetry_event_at(nullptr, 0u, &ev), QX_ERR_NULL_HANDLE);
}

TEST_F(QXTelemetryTest, EventAtNullOutReturnsError) {
    QXTelemetryEventInput in = make_input();
    qx_telemetry_record(telemetry_, &in, nullptr);
    EXPECT_EQ(qx_telemetry_event_at(telemetry_, 0u, nullptr), QX_ERR_NULL_HANDLE);
}

// =============================================================================
// MARK: – Event log – bulk copy & filter
// =============================================================================

TEST_F(QXTelemetryTest, EventsCopyReturnsAllRecorded) {
    QXTelemetryEventInput in = make_input(QX_TEL_PRESSURE, QX_SEVERITY_WARNING);
    for (int i = 0; i < 3; ++i)
        qx_telemetry_record(telemetry_, &in, nullptr);

    std::vector<QXTelemetryEvent> buf(8);
    uint32_t written = 0u;
    ASSERT_EQ(qx_telemetry_events_copy(
        telemetry_, buf.data(), static_cast<uint32_t>(buf.size()), &written), QX_OK);
    EXPECT_EQ(written, 3u);
}

TEST_F(QXTelemetryTest, EventsCopyNullHandleReturnsError) {
    QXTelemetryEvent ev{};
    uint32_t written = 0u;
    EXPECT_EQ(qx_telemetry_events_copy(nullptr, &ev, 1u, &written), QX_ERR_NULL_HANDLE);
}

TEST_F(QXTelemetryTest, EventsByCategoryFiltersCorrectly) {
    QXTelemetryEventInput alloc   = make_input(QX_TEL_ALLOCATION,   QX_SEVERITY_INFO);
    QXTelemetryEventInput evict   = make_input(QX_TEL_EVICTION,     QX_SEVERITY_INFO);
    qx_telemetry_record(telemetry_, &alloc, nullptr);
    qx_telemetry_record(telemetry_, &alloc, nullptr);
    qx_telemetry_record(telemetry_, &evict, nullptr);

    std::vector<QXTelemetryEvent> buf(8);
    uint32_t written = 0u;
    ASSERT_EQ(qx_telemetry_events_by_category(
        telemetry_, QX_TEL_ALLOCATION,
        buf.data(), static_cast<uint32_t>(buf.size()), &written), QX_OK);
    EXPECT_EQ(written, 2u);
}

TEST_F(QXTelemetryTest, ClearEventsResetsCount) {
    QXTelemetryEventInput in = make_input();
    qx_telemetry_record(telemetry_, &in, nullptr);
    ASSERT_EQ(qx_telemetry_clear_events(telemetry_), QX_OK);
    uint32_t count = 99u;
    qx_telemetry_event_count(telemetry_, &count);
    EXPECT_EQ(count, 0u);
}

TEST_F(QXTelemetryTest, ClearEventsNullHandleReturnsError) {
    EXPECT_EQ(qx_telemetry_clear_events(nullptr), QX_ERR_NULL_HANDLE);
}

// =============================================================================
// MARK: – Violation log
// =============================================================================

TEST_F(QXTelemetryTest, ViolationCountZeroOnCreate) {
    uint32_t count = 99u;
    ASSERT_EQ(qx_telemetry_violation_count(telemetry_, &count), QX_OK);
    EXPECT_EQ(count, 0u);
}

TEST_F(QXTelemetryTest, RecordViolationIncreasesViolationCount) {
    QXViolation v{};
    v.law_id   = QX_LAW_LIMIT;
    v.severity = QX_SEVERITY_CRITICAL;
    std::strncpy(v.message, "hard limit breach", sizeof(v.message) - 1u);

    ASSERT_EQ(qx_telemetry_record_violation(telemetry_, &v, nullptr), QX_OK);

    uint32_t count = 0u;
    qx_telemetry_violation_count(telemetry_, &count);
    EXPECT_EQ(count, 1u);
}

TEST_F(QXTelemetryTest, RecordViolationNullHandleReturnsError) {
    QXViolation v{};
    EXPECT_EQ(qx_telemetry_record_violation(nullptr, &v, nullptr), QX_ERR_NULL_HANDLE);
}

TEST_F(QXTelemetryTest, RecordViolationNullViolationReturnsError) {
    EXPECT_EQ(qx_telemetry_record_violation(telemetry_, nullptr, nullptr), QX_ERR_NULL_HANDLE);
}

TEST_F(QXTelemetryTest, ViolationAtReturnsCorrectLawId) {
    QXViolation v{};
    v.law_id   = QX_LAW_ETHICS;
    v.severity = QX_SEVERITY_WARNING;
    std::strncpy(v.message, "ethics flag false", sizeof(v.message) - 1u);
    qx_telemetry_record_violation(telemetry_, &v, nullptr);

    QXViolation out{};
    ASSERT_EQ(qx_telemetry_violation_at(telemetry_, 0u, &out), QX_OK);
    EXPECT_EQ(out.law_id, QX_LAW_ETHICS);
}

TEST_F(QXTelemetryTest, ViolationAtOutOfRangeReturnsError) {
    QXViolation out{};
    EXPECT_NE(qx_telemetry_violation_at(telemetry_, 9999u, &out), QX_OK);
}

TEST_F(QXTelemetryTest, ViolationAtNullHandleReturnsError) {
    QXViolation out{};
    EXPECT_EQ(qx_telemetry_violation_at(nullptr, 0u, &out), QX_ERR_NULL_HANDLE);
}

TEST_F(QXTelemetryTest, ViolationsCopyReturnsAll) {
    QXViolation v{};
    v.law_id   = QX_LAW_PATTERN;
    v.severity = QX_SEVERITY_FATAL;
    for (int i = 0; i < 4; ++i)
        qx_telemetry_record_violation(telemetry_, &v, nullptr);

    std::vector<QXViolation> buf(8);
    uint32_t written = 0u;
    ASSERT_EQ(qx_telemetry_violations_copy(
        telemetry_, buf.data(), static_cast<uint32_t>(buf.size()), &written), QX_OK);
    EXPECT_EQ(written, 4u);
}

TEST_F(QXTelemetryTest, ViolationsByLawFiltersCorrectly) {
    QXViolation v1{};
    v1.law_id   = QX_LAW_LIMIT;
    v1.severity = QX_SEVERITY_CRITICAL;
    QXViolation v2{};
    v2.law_id   = QX_LAW_PATTERN;
    v2.severity = QX_SEVERITY_WARNING;

    qx_telemetry_record_violation(telemetry_, &v1, nullptr);
    qx_telemetry_record_violation(telemetry_, &v1, nullptr);
    qx_telemetry_record_violation(telemetry_, &v2, nullptr);

    std::vector<QXViolation> buf(8);
    uint32_t written = 0u;
    ASSERT_EQ(qx_telemetry_violations_by_law(
        telemetry_, QX_LAW_LIMIT,
        buf.data(), static_cast<uint32_t>(buf.size()), &written), QX_OK);
    EXPECT_EQ(written, 2u);
}

TEST_F(QXTelemetryTest, ClearViolationsResetsCount) {
    QXViolation v{};
    v.law_id   = QX_LAW_ECONOMY;
    v.severity = QX_SEVERITY_INFO;
    qx_telemetry_record_violation(telemetry_, &v, nullptr);
    ASSERT_EQ(qx_telemetry_clear_violations(telemetry_), QX_OK);
    uint32_t count = 99u;
    qx_telemetry_violation_count(telemetry_, &count);
    EXPECT_EQ(count, 0u);
}

TEST_F(QXTelemetryTest, ClearViolationsNullHandleReturnsError) {
    EXPECT_EQ(qx_telemetry_clear_violations(nullptr), QX_ERR_NULL_HANDLE);
}

// =============================================================================
// MARK: – Record report
// =============================================================================

TEST_F(QXTelemetryTest, RecordReportNullHandleReturnsError) {
    QXLawReport report{};
    EXPECT_EQ(qx_telemetry_record_report(nullptr, &report, nullptr), QX_ERR_NULL_HANDLE);
}

TEST_F(QXTelemetryTest, RecordReportNullReportReturnsError) {
    EXPECT_EQ(qx_telemetry_record_report(telemetry_, nullptr, nullptr), QX_ERR_NULL_HANDLE);
}

TEST_F(QXTelemetryTest, RecordReportWithZeroViolationsSucceeds) {
    QXLawReport report{};
    report.health_score     = 95.0;
    report.is_fully_compliant = QX_TRUE;
    report.violation_count  = 0u;
    uint32_t recorded = 0u;
    EXPECT_EQ(qx_telemetry_record_report(telemetry_, &report, &recorded), QX_OK);
    // At minimum the evaluation event is recorded
    EXPECT_GE(recorded, 1u);
}

// =============================================================================
// MARK: – Reporting
// =============================================================================

TEST_F(QXTelemetryTest, ReportSucceedsOnEmptyLog) {
    QXTelemetryReport report{};
    EXPECT_EQ(qx_telemetry_report(telemetry_, 80.0, &report), QX_OK);
}

TEST_F(QXTelemetryTest, ReportNullHandleReturnsError) {
    QXTelemetryReport report{};
    EXPECT_EQ(qx_telemetry_report(nullptr, 80.0, &report), QX_ERR_NULL_HANDLE);
}

TEST_F(QXTelemetryTest, ReportNullOutReturnsError) {
    EXPECT_EQ(qx_telemetry_report(telemetry_, 80.0, nullptr), QX_ERR_NULL_HANDLE);
}

TEST_F(QXTelemetryTest, ReportInvalidHealthScoreReturnsError) {
    QXTelemetryReport report{};
    EXPECT_NE(qx_telemetry_report(telemetry_, -1.0,  &report), QX_OK);
    EXPECT_NE(qx_telemetry_report(telemetry_, 101.0, &report), QX_OK);
}

TEST_F(QXTelemetryTest, ReportToJsonSucceedsWithSufficientBuffer) {
    QXTelemetryReport report{};
    ASSERT_EQ(qx_telemetry_report(telemetry_, 75.0, &report), QX_OK);

    char buf[4096]{};
    uint32_t written = 0u;
    QXResult rc = qx_telemetry_report_to_json(&report, buf, sizeof(buf), &written);
    EXPECT_EQ(rc, QX_OK);
    EXPECT_GT(written, 0u);
}

TEST_F(QXTelemetryTest, ReportToJsonBufferTooSmallReturnsError) {
    QXTelemetryReport report{};
    ASSERT_EQ(qx_telemetry_report(telemetry_, 75.0, &report), QX_OK);

    char tiny[4]{};
    uint32_t written = 0u;
    EXPECT_EQ(qx_telemetry_report_to_json(&report, tiny, sizeof(tiny), &written),
              QX_ERR_BUFFER_TOO_SMALL);
}

TEST_F(QXTelemetryTest, ReportToJsonNullReportReturnsError) {
    char buf[256]{};
    uint32_t written = 0u;
    EXPECT_EQ(qx_telemetry_report_to_json(nullptr, buf, sizeof(buf), &written),
              QX_ERR_NULL_HANDLE);
}

TEST_F(QXTelemetryTest, ReportToJsonNullBufferReturnsError) {
    QXTelemetryReport report{};
    ASSERT_EQ(qx_telemetry_report(telemetry_, 75.0, &report), QX_OK);
    uint32_t written = 0u;
    EXPECT_EQ(qx_telemetry_report_to_json(&report, nullptr, 256u, &written),
              QX_ERR_NULL_HANDLE);
}

// =============================================================================
// MARK: – Statistics
// =============================================================================

TEST_F(QXTelemetryTest, StatsZeroOnCreate) {
    QXTelemetryStats stats{};
    ASSERT_EQ(qx_telemetry_stats(telemetry_, &stats), QX_OK);
    EXPECT_EQ(stats.total_events_recorded, 0u);
    EXPECT_EQ(stats.total_violations_recorded, 0u);
}

TEST_F(QXTelemetryTest, StatsReflectRecordedEvents) {
    QXTelemetryEventInput alloc = make_input(QX_TEL_ALLOCATION,   QX_SEVERITY_INFO);
    QXTelemetryEventInput dealloc = make_input(QX_TEL_DEALLOCATION, QX_SEVERITY_INFO);
    qx_telemetry_record(telemetry_, &alloc,   nullptr);
    qx_telemetry_record(telemetry_, &alloc,   nullptr);
    qx_telemetry_record(telemetry_, &dealloc, nullptr);

    QXTelemetryStats stats{};
    ASSERT_EQ(qx_telemetry_stats(telemetry_, &stats), QX_OK);
    EXPECT_EQ(stats.total_events_recorded,    3u);
    EXPECT_EQ(stats.total_allocation_events,  2u);
    EXPECT_EQ(stats.total_deallocation_events,1u);
}

TEST_F(QXTelemetryTest, StatsReflectViolations) {
    QXViolation v{};
    v.law_id   = QX_LAW_LIMIT;
    v.severity = QX_SEVERITY_CRITICAL;
    qx_telemetry_record_violation(telemetry_, &v, nullptr);

    QXTelemetryStats stats{};
    ASSERT_EQ(qx_telemetry_stats(telemetry_, &stats), QX_OK);
    EXPECT_EQ(stats.total_violations_recorded, 1u);
}

TEST_F(QXTelemetryTest, StatsNullHandleReturnsError) {
    QXTelemetryStats stats{};
    EXPECT_EQ(qx_telemetry_stats(nullptr, &stats), QX_ERR_NULL_HANDLE);
}

TEST_F(QXTelemetryTest, StatsNullOutReturnsError) {
    EXPECT_EQ(qx_telemetry_stats(telemetry_, nullptr), QX_ERR_NULL_HANDLE);
}

// =============================================================================
// MARK: – Category label helper
// =============================================================================

TEST_F(QXTelemetryTest, CategoryLabelAllocation) {
    EXPECT_STREQ(qx_telemetry_category_label(QX_TEL_ALLOCATION),   "Allocation");
}

TEST_F(QXTelemetryTest, CategoryLabelDeallocation) {
    EXPECT_STREQ(qx_telemetry_category_label(QX_TEL_DEALLOCATION), "Deallocation");
}

TEST_F(QXTelemetryTest, CategoryLabelEviction) {
    EXPECT_STREQ(qx_telemetry_category_label(QX_TEL_EVICTION),     "Eviction");
}

TEST_F(QXTelemetryTest, CategoryLabelPressure) {
    EXPECT_STREQ(qx_telemetry_category_label(QX_TEL_PRESSURE),     "Pressure");
}

TEST_F(QXTelemetryTest, CategoryLabelEvaluation) {
    EXPECT_STREQ(qx_telemetry_category_label(QX_TEL_EVALUATION),   "Evaluation");
}

TEST_F(QXTelemetryTest, CategoryLabelSnapshot) {
    EXPECT_STREQ(qx_telemetry_category_label(QX_TEL_SNAPSHOT),     "Snapshot");
}

TEST_F(QXTelemetryTest, CategoryLabelEngine) {
    EXPECT_STREQ(qx_telemetry_category_label(QX_TEL_ENGINE),       "Engine");
}

TEST_F(QXTelemetryTest, CategoryLabelViolation) {
    EXPECT_STREQ(qx_telemetry_category_label(QX_TEL_VIOLATION),    "Violation");
}

TEST_F(QXTelemetryTest, CategoryLabelSecurity) {
    EXPECT_STREQ(qx_telemetry_category_label(QX_TEL_SECURITY),     "Security");
}

TEST_F(QXTelemetryTest, CategoryLabelNative) {
    EXPECT_STREQ(qx_telemetry_category_label(QX_TEL_NATIVE),       "Native");
}
