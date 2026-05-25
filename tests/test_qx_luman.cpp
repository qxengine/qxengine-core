/**
 * @file test_qx_luman.cpp
 * @brief LUMAN — Constitutional Budget Tests
 *
 * ALAMTOLOGI — QURANIC SCIENCE
 * Founder: Masa Bayu
 *
 * Every test verifies one constitutional mathematical truth.
 * Numbers are derived from LUMAN and verified exactly.
 *
 * Repository: https://github.com/qxengine/qxengine-core
 * Date:       2026-05-25
 * Version:    1.0.0-rc.2
 */

#include <gtest/gtest.h>

extern "C" {
#include "qxengine/memory/qx_luman.h"
}

TEST(LumanPattern, Pola4_Sa_SingleBox) {
    QXLumanPattern p{};
    ASSERT_EQ(qx_luman_compute_pattern(0, 4, &p), QX_OK);
    EXPECT_EQ(p.total_columns, 1);
    EXPECT_EQ(p.columns[0], 1ULL);
    EXPECT_EQ(p.total_boxes, 1ULL);
    EXPECT_EQ(p.core_value, 1ULL);
    EXPECT_STREQ(p.symbol, "4 0 0");
    EXPECT_STREQ(p.level_name, "Sa");
}

TEST(LumanPattern, Pola4_Du_Structure_1_4_1) {
    QXLumanPattern p{};
    ASSERT_EQ(qx_luman_compute_pattern(1, 4, &p), QX_OK);
    EXPECT_EQ(p.total_columns, 3);
    EXPECT_EQ(p.columns[0], 1ULL);
    EXPECT_EQ(p.columns[1], 4ULL);
    EXPECT_EQ(p.columns[2], 1ULL);
    EXPECT_EQ(p.total_boxes, 6ULL);
    EXPECT_EQ(p.core_value, 4ULL);
    EXPECT_STREQ(p.symbol, "4 0 1");
    EXPECT_STREQ(p.level_name, "Du");
}

TEST(LumanPattern, Pola4_Ga_Structure_1_4_16_4_1) {
    QXLumanPattern p{};
    ASSERT_EQ(qx_luman_compute_pattern(2, 4, &p), QX_OK);
    EXPECT_EQ(p.total_columns, 5);
    EXPECT_EQ(p.columns[0], 1ULL);
    EXPECT_EQ(p.columns[1], 4ULL);
    EXPECT_EQ(p.columns[2], 16ULL);
    EXPECT_EQ(p.columns[3], 4ULL);
    EXPECT_EQ(p.columns[4], 1ULL);
    EXPECT_EQ(p.total_boxes, 26ULL);
    EXPECT_EQ(p.core_value, 16ULL);
}

TEST(LumanPattern, Pola4_HigherTotals) {
    QXLumanPattern p{};
    ASSERT_EQ(qx_luman_compute_pattern(3, 4, &p), QX_OK);
    EXPECT_EQ(p.total_boxes, 106ULL);
    EXPECT_EQ(p.core_value, 64ULL);
    EXPECT_EQ(p.columns[3], 64ULL);

    ASSERT_EQ(qx_luman_compute_pattern(4, 4, &p), QX_OK);
    EXPECT_EQ(p.total_boxes, 426ULL);
    EXPECT_EQ(p.core_value, 256ULL);

    ASSERT_EQ(qx_luman_compute_pattern(5, 4, &p), QX_OK);
    EXPECT_EQ(p.total_boxes, 1706ULL);
    EXPECT_EQ(p.core_value, 1024ULL);
}

TEST(LumanPattern, Pola6_Sa_SingleBox) {
    QXLumanPattern p{};
    ASSERT_EQ(qx_luman_compute_pattern(0, 6, &p), QX_OK);
    EXPECT_EQ(p.total_boxes, 1ULL);
    EXPECT_EQ(p.core_value, 1ULL);
    EXPECT_STREQ(p.symbol, "6 0 0");
}

TEST(LumanPattern, Pola6_Du_Structure_1_6_1) {
    QXLumanPattern p{};
    ASSERT_EQ(qx_luman_compute_pattern(1, 6, &p), QX_OK);
    EXPECT_EQ(p.total_columns, 3);
    EXPECT_EQ(p.columns[0], 1ULL);
    EXPECT_EQ(p.columns[1], 6ULL);
    EXPECT_EQ(p.columns[2], 1ULL);
    EXPECT_EQ(p.total_boxes, 8ULL);
}

TEST(LumanPattern, Pola6_Ga_Structure_1_6_36_6_1) {
    QXLumanPattern p{};
    ASSERT_EQ(qx_luman_compute_pattern(2, 6, &p), QX_OK);
    EXPECT_EQ(p.columns[0], 1ULL);
    EXPECT_EQ(p.columns[1], 6ULL);
    EXPECT_EQ(p.columns[2], 36ULL);
    EXPECT_EQ(p.columns[3], 6ULL);
    EXPECT_EQ(p.columns[4], 1ULL);
    EXPECT_EQ(p.total_boxes, 50ULL);
}

TEST(LumanPattern, Pola6_HigherTotals) {
    QXLumanPattern p{};
    ASSERT_EQ(qx_luman_compute_pattern(3, 6, &p), QX_OK);
    EXPECT_EQ(p.total_boxes, 302ULL);
    EXPECT_EQ(p.core_value, 216ULL);

    ASSERT_EQ(qx_luman_compute_pattern(4, 6, &p), QX_OK);
    EXPECT_EQ(p.total_boxes, 1814ULL);
    EXPECT_EQ(p.core_value, 1296ULL);
}

TEST(LumanPattern, InvalidInputsRejected) {
    QXLumanPattern p{};
    EXPECT_NE(qx_luman_compute_pattern(0, 5, &p), QX_OK);
    EXPECT_NE(qx_luman_compute_pattern(0, 3, &p), QX_OK);
    EXPECT_NE(qx_luman_compute_pattern(7, 4, &p), QX_OK);
    EXPECT_NE(qx_luman_compute_pattern(0, 4, nullptr), QX_OK);
}

TEST(LumanPeringkat, DetectAllLevels) {
    uint8_t p = 0u;

    ASSERT_EQ(qx_luman_detect_peringkat(512ULL * 1024 * 1024, &p), QX_OK);
    EXPECT_EQ(p, QX_LUMAN_PERINGKAT_SA);

    ASSERT_EQ(qx_luman_detect_peringkat(2ULL * 1024 * 1024 * 1024, &p), QX_OK);
    EXPECT_EQ(p, QX_LUMAN_PERINGKAT_DU);

    ASSERT_EQ(qx_luman_detect_peringkat(4ULL * 1024 * 1024 * 1024, &p), QX_OK);
    EXPECT_EQ(p, QX_LUMAN_PERINGKAT_GA);

    ASSERT_EQ(qx_luman_detect_peringkat(6ULL * 1024 * 1024 * 1024, &p), QX_OK);
    EXPECT_EQ(p, QX_LUMAN_PERINGKAT_PA);

    ASSERT_EQ(qx_luman_detect_peringkat(8ULL * 1024 * 1024 * 1024, &p), QX_OK);
    EXPECT_EQ(p, QX_LUMAN_PERINGKAT_MA);

    ASSERT_EQ(qx_luman_detect_peringkat(12ULL * 1024 * 1024 * 1024, &p), QX_OK);
    EXPECT_EQ(p, QX_LUMAN_PERINGKAT_NA);

    ASSERT_EQ(qx_luman_detect_peringkat(32ULL * 1024 * 1024 * 1024, &p), QX_OK);
    EXPECT_EQ(p, QX_LUMAN_PERINGKAT_TU);
}

TEST(LumanPeringkat, NullOutputRejected) {
    EXPECT_NE(qx_luman_detect_peringkat(
        4ULL * 1024 * 1024 * 1024, nullptr), QX_OK);
}

TEST(LumanBudget, SegmentBudgetsMatchLumanMath) {
    QXLumanBudget b{};

    ASSERT_EQ(qx_luman_budget_for_segment(QX_LUMAN_PERINGKAT_GA, "IMG", &b), QX_OK);
    EXPECT_EQ(b.soft_bytes, 26ULL * 1024 * 1024);
    EXPECT_EQ(b.core_bytes, 16ULL * 1024 * 1024);
    EXPECT_EQ(b.pola_base, 4);

    ASSERT_EQ(qx_luman_budget_for_segment(QX_LUMAN_PERINGKAT_PA, "IMG", &b), QX_OK);
    EXPECT_EQ(b.soft_bytes, 106ULL * 1024 * 1024);
    EXPECT_EQ(b.core_bytes, 64ULL * 1024 * 1024);

    ASSERT_EQ(qx_luman_budget_for_segment(QX_LUMAN_PERINGKAT_GA, "AV", &b), QX_OK);
    EXPECT_EQ(b.soft_bytes, 50ULL * 1024 * 1024);
    EXPECT_EQ(b.core_bytes, 36ULL * 1024 * 1024);
    EXPECT_EQ(b.pola_base, 6);

    ASSERT_EQ(qx_luman_budget_for_segment(QX_LUMAN_PERINGKAT_PA, "AV", &b), QX_OK);
    EXPECT_EQ(b.soft_bytes, 302ULL * 1024 * 1024);
    EXPECT_EQ(b.core_bytes, 216ULL * 1024 * 1024);
}

TEST(LumanBudget, HardBudgetIsOnePointFiveSoft) {
    QXLumanBudget b{};
    ASSERT_EQ(qx_luman_budget_for_segment(QX_LUMAN_PERINGKAT_PA, "IMG", &b), QX_OK);

    const double ratio = static_cast<double>(b.hard_bytes) /
                         static_cast<double>(b.soft_bytes);
    EXPECT_NEAR(ratio, 1.5, 0.001);
}

TEST(LumanBudget, SlotBudgetsSumToSoftBudget) {
    QXLumanBudget b{};
    QXSize total = 0u;

    ASSERT_EQ(qx_luman_budget_for_segment(QX_LUMAN_PERINGKAT_GA, "IMG", &b), QX_OK);

    for (uint8_t i = 0; i < b.slot_count; ++i) {
        total += b.slot_budgets[i];
    }

    EXPECT_EQ(total, b.soft_bytes);
}

TEST(LumanBudget, AllSegmentsComputedAtPa) {
    QXLumanBudget budgets[16]{};
    uint8_t count = 0u;

    ASSERT_EQ(qx_luman_budget_all_segments(
        QX_LUMAN_PERINGKAT_PA, budgets, &count), QX_OK);
    EXPECT_GT(count, 0u);

    for (uint8_t i = 0u; i < count; ++i) {
        EXPECT_GT(budgets[i].soft_bytes, 0ULL)
            << "Segment " << budgets[i].segment_id << " has zero soft budget";
    }
}

TEST(LumanBudget, UnknownSegmentReturnsNotFound) {
    QXLumanBudget b{};
    EXPECT_EQ(qx_luman_budget_for_segment(
        QX_LUMAN_PERINGKAT_GA, "UNKNOWN", &b), QX_ERR_LEAF_NOT_FOUND);
}

TEST(LumanBudget, XConstantMatchesSegmentDefinition) {
    QXLumanBudget b{};

    ASSERT_EQ(qx_luman_budget_for_segment(QX_LUMAN_PERINGKAT_GA, "AV", &b), QX_OK);
    EXPECT_DOUBLE_EQ(b.declared_x, 50.0);

    ASSERT_EQ(qx_luman_budget_for_segment(QX_LUMAN_PERINGKAT_GA, "IMG", &b), QX_OK);
    EXPECT_DOUBLE_EQ(b.declared_x, 2000.0);

    ASSERT_EQ(qx_luman_budget_for_segment(QX_LUMAN_PERINGKAT_GA, "NET", &b), QX_OK);
    EXPECT_DOUBLE_EQ(b.declared_x, 500.0);
}

TEST(LumanProfile, Build8GBDevice) {
    QXLumanDeviceProfile profile{};
    const uint64_t ram = 8ULL * 1024 * 1024 * 1024;

    ASSERT_EQ(qx_luman_build_device_profile(ram, &profile), QX_OK);
    EXPECT_EQ(profile.peringkat, QX_LUMAN_PERINGKAT_MA);
    EXPECT_STREQ(profile.peringkat_name, "Ma");
    EXPECT_EQ(profile.physical_ram_bytes, ram);
    EXPECT_EQ(profile.auto_detected, QX_TRUE);
}

TEST(LumanProfile, InvalidInputsRejected) {
    QXLumanDeviceProfile profile{};
    EXPECT_NE(qx_luman_build_device_profile(0, &profile), QX_OK);
    EXPECT_NE(qx_luman_build_device_profile(
        8ULL * 1024 * 1024 * 1024, nullptr), QX_OK);
}
