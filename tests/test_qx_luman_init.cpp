/**
 * @file test_qx_luman_init.cpp
 * @brief LUMAN Init — Constitutional Integration Tests
 *
 * ALAMTOLOGI — QURANIC SCIENCE
 * Founder: Masa Bayu
 *
 * Tests the RAM -> Peringkat -> budget initialisation flow.
 *
 * Repository: https://github.com/qxengine/qxengine-core
 * Date:       2026-05-25
 * Version:    1.0.0-rc.3
 */

#include <gtest/gtest.h>

#include <cstring>

extern "C" {
#include "qxengine/memory/qx_luman_init.h"
}

namespace {

constexpr uint64_t kMiB = 1024ULL * 1024ULL;
constexpr uint64_t kGiB = 1024ULL * kMiB;

QXSize sum_soft(const QXLumanInitResult& result) {
    QXSize sum = 0u;
    for (uint8_t i = 0u; i < result.segment_count; ++i) {
        sum += result.segment_budgets[i].soft_bytes;
    }
    return sum;
}

QXSize sum_hard(const QXLumanInitResult& result) {
    QXSize sum = 0u;
    for (uint8_t i = 0u; i < result.segment_count; ++i) {
        sum += result.segment_budgets[i].hard_bytes;
    }
    return sum;
}

const QXLumanBudget* find_segment(
    const QXLumanInitResult& result,
    const char* segment_id) {
    for (uint8_t i = 0u; i < result.segment_count; ++i) {
        if (std::strcmp(result.segment_budgets[i].segment_id, segment_id) == 0) {
            return &result.segment_budgets[i];
        }
    }
    return nullptr;
}

} // namespace

TEST(LumanInit, Init8GBSucceeds) {
    QXLumanInitResult result{};
    ASSERT_EQ(qx_luman_init_with_ram(8ULL * kGiB, &result), QX_OK);
    EXPECT_EQ(result.initialized, QX_TRUE);
    EXPECT_EQ(result.device_profile.peringkat, QX_LUMAN_PERINGKAT_MA);
}

TEST(LumanInit, Init4GBPeringkatGa) {
    QXLumanInitResult result{};
    ASSERT_EQ(qx_luman_init_with_ram(4ULL * kGiB, &result), QX_OK);
    EXPECT_EQ(result.device_profile.peringkat, QX_LUMAN_PERINGKAT_GA);
    EXPECT_STREQ(result.device_profile.peringkat_name, "Ga");
}

TEST(LumanInit, InitZeroRAMFails) {
    QXLumanInitResult result{};
    EXPECT_NE(qx_luman_init_with_ram(0u, &result), QX_OK);
}

TEST(LumanInit, InitNullResultFails) {
    EXPECT_NE(qx_luman_init_with_ram(4ULL * kGiB, nullptr), QX_OK);
}

TEST(LumanInit, InitSegmentCountNonZero) {
    QXLumanInitResult result{};
    ASSERT_EQ(qx_luman_init_with_ram(8ULL * kGiB, &result), QX_OK);
    EXPECT_GT(result.segment_count, 0u);
}

TEST(LumanInit, InitLogBufferNonEmpty) {
    QXLumanInitResult result{};
    ASSERT_EQ(qx_luman_init_with_ram(8ULL * kGiB, &result), QX_OK);
    EXPECT_GT(std::strlen(result.log_buffer), 0u);
}

TEST(LumanInit, InitAutoDetectSucceeds) {
    QXLumanInitResult result{};
    ASSERT_EQ(qx_luman_init(&result), QX_OK);
    EXPECT_EQ(result.initialized, QX_TRUE);
}

TEST(LumanInit, ProfileDescriptionNonEmpty) {
    QXLumanInitResult result{};
    ASSERT_EQ(qx_luman_init_with_ram(8ULL * kGiB, &result), QX_OK);
    EXPECT_GT(std::strlen(result.device_profile.device_description), 0u);
}

TEST(LumanInit, AVGaSoftBudget50MB) {
    QXLumanInitResult result{};
    ASSERT_EQ(qx_luman_init_with_ram(4ULL * kGiB, &result), QX_OK);
    EXPECT_EQ(result.av_budget.soft_bytes, 50ULL * kMiB);
    EXPECT_EQ(result.av_budget.pola_base, QX_LUMAN_POLA_6);
}

TEST(LumanInit, AVPaSoftBudget302MB) {
    QXLumanInitResult result{};
    ASSERT_EQ(qx_luman_init_with_ram(6ULL * kGiB, &result), QX_OK);
    EXPECT_EQ(result.av_budget.soft_bytes, 302ULL * kMiB);
}

TEST(LumanInit, AVMaSoftBudget1814MB) {
    QXLumanInitResult result{};
    ASSERT_EQ(qx_luman_init_with_ram(8ULL * kGiB, &result), QX_OK);
    EXPECT_EQ(result.av_budget.soft_bytes, 1814ULL * kMiB);
}

TEST(LumanInit, AVXConstantIs50ms) {
    QXLumanInitResult result{};
    ASSERT_EQ(qx_luman_init_with_ram(8ULL * kGiB, &result), QX_OK);
    EXPECT_DOUBLE_EQ(result.av_budget.declared_x, 50.0);
}

TEST(LumanInit, AVBudgetAppearsInAllSegments) {
    QXLumanInitResult result{};
    ASSERT_EQ(qx_luman_init_with_ram(4ULL * kGiB, &result), QX_OK);
    ASSERT_NE(find_segment(result, "AV"), nullptr);
}

TEST(LumanInit, IMGGaSoftBudget26MB) {
    QXLumanInitResult result{};
    ASSERT_EQ(qx_luman_init_with_ram(4ULL * kGiB, &result), QX_OK);
    EXPECT_EQ(result.img_budget.soft_bytes, 26ULL * kMiB);
    EXPECT_EQ(result.img_budget.pola_base, QX_LUMAN_POLA_4);
}

TEST(LumanInit, IMGPaSoftBudget106MB) {
    QXLumanInitResult result{};
    ASSERT_EQ(qx_luman_init_with_ram(6ULL * kGiB, &result), QX_OK);
    EXPECT_EQ(result.img_budget.soft_bytes, 106ULL * kMiB);
}

TEST(LumanInit, IMGMaSoftBudget426MB) {
    QXLumanInitResult result{};
    ASSERT_EQ(qx_luman_init_with_ram(8ULL * kGiB, &result), QX_OK);
    EXPECT_EQ(result.img_budget.soft_bytes, 426ULL * kMiB);
}

TEST(LumanInit, IMGXConstantIs2000ms) {
    QXLumanInitResult result{};
    ASSERT_EQ(qx_luman_init_with_ram(8ULL * kGiB, &result), QX_OK);
    EXPECT_DOUBLE_EQ(result.img_budget.declared_x, 2000.0);
}

TEST(LumanInit, NETGaSoftBudget26MB) {
    QXLumanInitResult result{};
    ASSERT_EQ(qx_luman_init_with_ram(4ULL * kGiB, &result), QX_OK);
    EXPECT_EQ(result.net_budget.soft_bytes, 26ULL * kMiB);
    EXPECT_EQ(result.net_budget.pola_base, QX_LUMAN_POLA_4);
}

TEST(LumanInit, NETXConstantIs500ms) {
    QXLumanInitResult result{};
    ASSERT_EQ(qx_luman_init_with_ram(8ULL * kGiB, &result), QX_OK);
    EXPECT_DOUBLE_EQ(result.net_budget.declared_x, 500.0);
}

TEST(LumanInit, TotalSoftEqualsSumOfSegments) {
    QXLumanInitResult result{};
    ASSERT_EQ(qx_luman_init_with_ram(8ULL * kGiB, &result), QX_OK);
    EXPECT_EQ(result.total_soft_bytes, sum_soft(result));
}

TEST(LumanInit, TotalHardEqualsSumOfSegments) {
    QXLumanInitResult result{};
    ASSERT_EQ(qx_luman_init_with_ram(8ULL * kGiB, &result), QX_OK);
    EXPECT_EQ(result.total_hard_bytes, sum_hard(result));
}

TEST(LumanInit, TotalHardGreaterThanTotalSoft) {
    QXLumanInitResult result{};
    ASSERT_EQ(qx_luman_init_with_ram(8ULL * kGiB, &result), QX_OK);
    EXPECT_GT(result.total_hard_bytes, result.total_soft_bytes);
}

TEST(LumanInit, AllPeringkatProduceValidTotals) {
    const uint64_t ram_values[] = {
        512ULL * kMiB, 2ULL * kGiB, 4ULL * kGiB, 6ULL * kGiB,
        8ULL * kGiB, 12ULL * kGiB, 32ULL * kGiB
    };

    for (uint64_t ram : ram_values) {
        QXLumanInitResult result{};
        ASSERT_EQ(qx_luman_init_with_ram(ram, &result), QX_OK);
        EXPECT_GT(result.total_soft_bytes, 0ULL);
        EXPECT_GT(result.total_hard_bytes, result.total_soft_bytes);
    }
}

TEST(LumanInit, EverySegmentHardBudgetExceedsSoft) {
    QXLumanInitResult result{};
    ASSERT_EQ(qx_luman_init_with_ram(4ULL * kGiB, &result), QX_OK);

    for (uint8_t i = 0u; i < result.segment_count; ++i) {
        EXPECT_GT(result.segment_budgets[i].hard_bytes,
                  result.segment_budgets[i].soft_bytes);
    }
}

TEST(LumanInit, SegmentCountMatchesDefinitions) {
    QXLumanInitResult result{};
    ASSERT_EQ(qx_luman_init_with_ram(4ULL * kGiB, &result), QX_OK);
    EXPECT_EQ(result.segment_count, 10u);
}
