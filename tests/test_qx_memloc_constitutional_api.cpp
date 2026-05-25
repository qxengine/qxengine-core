// =============================================================================
// test_qx_memloc_constitutional_api.cpp
// QXEngine Core – Constitutional Memory Public API Tests
// =============================================================================
// Owner : Masa Bayu
// Created: 2026-05-25
// Repo   : https://github.com/qxengine/qxengine-core
// Path   : tests/test_qx_memloc_constitutional_api.cpp
// License: Apache 2.0
//

#include "qxengine/memory/qx_memloc_constitutional.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstring>

namespace {

constexpr QXSize kSoftLimit = 64ULL * 1024ULL * 1024ULL;
constexpr QXSize kHardLimit = 128ULL * 1024ULL * 1024ULL;
constexpr QXSize kAllocSize = 4096ULL;

QXTimestamp now_ms() {
    const auto now = std::chrono::system_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch());
    return static_cast<QXTimestamp>(ms.count());
}

QXConstitutionalConfig make_config() {
    QXConstitutionalConfig cfg{};
    cfg.soft_budget_bytes = kSoftLimit;
    cfg.hard_budget_bytes = kHardLimit;
    cfg.device_scale = 1.0;
    cfg.declared_x = 1.0;
    cfg.tolerance_pct = 15.0;
    cfg.instance_label = "api-test-instance";
    return cfg;
}

QXAllocationRequest make_request(const char* label, const char* segment) {
    QXAllocationRequest req{};
    req.label = label;
    req.segment_id = segment;
    req.slot_id = "api-slot";
    req.size_bytes = kAllocSize;
    req.leaf_class = QX_LEAF_CLASS_C;
    req.purpose = "public API test allocation";
    return req;
}

}  // namespace

TEST(QXMemlocConstitutionalAPI, ConfigCreateSetsIdentityAndCertification) {
    QXMemlocConstitutional memloc{};
    const QXConstitutionalConfig cfg = make_config();

    ASSERT_EQ(qx_memloc_constitutional_create_config(&cfg, &memloc), QX_OK);
    EXPECT_STREQ(qx_memloc_constitutional_instance_id(&memloc),
                 "api-test-instance");
    EXPECT_GE(qx_memloc_constitutional_health_score(&memloc),
              QX_SCORE_SOVEREIGN);
    EXPECT_EQ(qx_memloc_constitutional_certification(&memloc),
              QX_CERTIFICATION_SOVEREIGN);

    qx_memloc_constitutional_destroy(&memloc);
}

TEST(QXMemlocConstitutionalAPI, AllocateAndDeallocateThroughWrapper) {
    QXMemlocConstitutional memloc{};
    const QXConstitutionalConfig cfg = make_config();
    ASSERT_EQ(qx_memloc_constitutional_create_config(
                  &cfg, &memloc), QX_OK);

    QXConstitutionalAllocation alloc{};
    const QXAllocationRequest req = make_request("api-img-leaf", "IMG");

    ASSERT_EQ(qx_memloc_constitutional_allocate(&memloc, &req, &alloc), QX_OK);
    EXPECT_NE(alloc.ptr, nullptr);
    EXPECT_NE(alloc.leaf_handle, nullptr);
    EXPECT_GE(alloc.usable_bytes, kAllocSize);
    EXPECT_STREQ(alloc.segment_id, QX_SEGMENT_IMG);
    EXPECT_DOUBLE_EQ(alloc.declared_x, 1.0);

    EXPECT_EQ(qx_memloc_constitutional_deallocate(
                  &memloc, alloc.leaf_handle), QX_OK);
    qx_memloc_constitutional_destroy(&memloc);
}

TEST(QXMemlocConstitutionalAPI, ReportProducesCycleTruth) {
    QXMemlocConstitutional memloc{};
    const QXConstitutionalConfig cfg = make_config();
    ASSERT_EQ(qx_memloc_constitutional_create_config(
                  &cfg, &memloc), QX_OK);

    QXConstitutionalAllocation alloc{};
    const QXAllocationRequest req = make_request(
        "api-report-leaf", QX_SEGMENT_DATA);
    ASSERT_EQ(qx_memloc_constitutional_allocate(&memloc, &req, &alloc), QX_OK);

    QXConstitutionalReport report{};
    ASSERT_EQ(qx_memloc_constitutional_report(
                  &memloc, now_ms(), &report), QX_OK);

    EXPECT_EQ(report.cycle_number, 1u);
    EXPECT_GT(report.aba_measurement.declared_x, 0.0);
    EXPECT_GE(report.health_score, QX_SCORE_MINIMUM);
    EXPECT_LE(report.health_score, 100.0);
    EXPECT_NE(report.certification, QX_CERTIFICATION_NONE);
    EXPECT_GE(report.active_leaf_count, 1u);

    qx_memloc_constitutional_deallocate(&memloc, alloc.leaf_handle);
    qx_memloc_constitutional_destroy(&memloc);
}

TEST(QXMemlocConstitutionalAPI, NotifyForwardersValidateAndAcceptSignals) {
    QXMemlocConstitutional memloc{};
    const QXConstitutionalConfig cfg = make_config();
    ASSERT_EQ(qx_memloc_constitutional_create_config(
                  &cfg, &memloc), QX_OK);

    EXPECT_EQ(qx_memloc_constitutional_notify_lifecycle(&memloc, 1u), QX_OK);
    EXPECT_EQ(qx_memloc_constitutional_notify_lifecycle(&memloc, 0u), QX_OK);
    EXPECT_EQ(qx_memloc_constitutional_notify_screen(&memloc, "home"), QX_OK);
    EXPECT_EQ(qx_memloc_constitutional_notify_pressure(&memloc, 1u), QX_OK);
    EXPECT_NE(qx_memloc_constitutional_notify_pressure(&memloc, 0u), QX_OK);
    EXPECT_NE(qx_memloc_constitutional_notify_screen(&memloc, nullptr), QX_OK);

    qx_memloc_constitutional_destroy(&memloc);
}
