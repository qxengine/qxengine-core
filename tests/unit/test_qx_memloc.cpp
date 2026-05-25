// =============================================================================
// QXEngine Core – tests/unit/test_qx_memloc.cpp
// Owner : Masa Bayu
// Created: 2026-05-24
// Repo   : https://github.com/qxengine/qxengine-core
// Description: Unit tests for QXMemloc – lifecycle, allocation, deallocation,
//              segment access, aggregate stats, and multi-segment behaviour.
// =============================================================================

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>

#include "qxengine/memory/qx_memloc.h"
#include "qxengine/memory/qx_leaf.h"
#include "qxengine/memory/qx_segment.h"
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
class QXMemlocTest : public ::testing::Test {
protected:
    QXMemlocHandle memloc = nullptr;

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
    }

    void TearDown() override {
        if (memloc) {
            qx_memloc_destroy(memloc);
            memloc = nullptr;
        }
    }

    // Allocate a leaf — returns result struct
    QXResult alloc(const char*    label,
                   const char*    segment_id,
                   QXLeafClassId  cls,
                   QXSize         size,
                   QXLeafHandle*  out_leaf = nullptr,
                   char           out_id[37] = nullptr) {
        QXAllocResult r{};
        QXResult rc = qx_memloc_allocate(memloc, label, segment_id,
                                          cls, size, &r);
        if (rc == QX_OK) {
            if (out_leaf) *out_leaf = r.leaf;
            if (out_id)   qx_leaf_id(r.leaf, out_id);
        }
        return rc;
    }
};

// ---------------------------------------------------------------------------
// MARK: – Lifecycle
// ---------------------------------------------------------------------------

TEST_F(QXMemlocTest, CreateSucceeds) {
    EXPECT_NE(memloc, nullptr);
}

TEST_F(QXMemlocTest, CreateNullOutHandleReturnsError) {
    QXSegmentConfig configs[QX_SEGMENT_COUNT]{};
    for (uint32_t i = 0; i < QX_SEGMENT_COUNT; ++i) {
        std::strncpy(configs[i].segment_id, kSegIds[i], 15);
        configs[i].effective_soft_bytes = kSoftLimit;
        configs[i].effective_hard_bytes = kHardLimit;
        configs[i].max_slots            = kMaxSlots;
    }
    EXPECT_NE(qx_memloc_create(configs, QX_SEGMENT_COUNT, nullptr), QX_OK);
}

TEST_F(QXMemlocTest, DestroyNullIsNoOp) {
    EXPECT_NO_FATAL_FAILURE(qx_memloc_destroy(nullptr));
}

// ---------------------------------------------------------------------------
// MARK: – Allocation
// ---------------------------------------------------------------------------

TEST_F(QXMemlocTest, AllocSucceeds) {
    QXLeafHandle lf = nullptr;
    EXPECT_EQ(alloc("alloc.test", QX_SEGMENT_CORE,
                    QX_LEAF_CLASS_D, 1024u, &lf), QX_OK);
    EXPECT_NE(lf, nullptr);
}

TEST_F(QXMemlocTest, AllocBlankLabelRejected) {
    EXPECT_NE(alloc("", QX_SEGMENT_CORE, QX_LEAF_CLASS_D, 1024u), QX_OK);
}

TEST_F(QXMemlocTest, AllocNullLabelRejected) {
    QXAllocResult r{};
    EXPECT_NE(qx_memloc_allocate(memloc, nullptr, QX_SEGMENT_CORE,
                                  QX_LEAF_CLASS_D, 1024u, &r), QX_OK);
}

TEST_F(QXMemlocTest, AllocUnknownSegmentRejected) {
    EXPECT_NE(alloc("test.leaf", "QLM-UNKNOWN",
                    QX_LEAF_CLASS_D, 1024u), QX_OK);
}

TEST_F(QXMemlocTest, AllocZeroSizeRejected) {
    QXAllocResult r{};
    EXPECT_NE(qx_memloc_allocate(memloc, "zero.size", QX_SEGMENT_CORE,
                                  QX_LEAF_CLASS_D, 0u, &r), QX_OK);
}

TEST_F(QXMemlocTest, AllocIncreasesTotalUsedBytes) {
    ASSERT_EQ(alloc("used.bytes", QX_SEGMENT_CORE,
                    QX_LEAF_CLASS_D, 4096u), QX_OK);
    QXSize used = 0u;
    ASSERT_EQ(qx_memloc_total_used_bytes(memloc, &used), QX_OK);
    EXPECT_EQ(used, 4096u);
}

TEST_F(QXMemlocTest, AllocResultContainsLeafHandle) {
    QXAllocResult r{};
    ASSERT_EQ(qx_memloc_allocate(memloc, "result.test", QX_SEGMENT_CORE,
                                  QX_LEAF_CLASS_D, 512u, &r), QX_OK);
    EXPECT_NE(r.leaf, nullptr);
    EXPECT_EQ(r.bytes_allocated, 512u);
}

// ---------------------------------------------------------------------------
// MARK: – Deallocation
// ---------------------------------------------------------------------------

TEST_F(QXMemlocTest, DeallocReducesTotalUsedBytes) {
    char id[37]{};
    ASSERT_EQ(alloc("free.test", QX_SEGMENT_CORE,
                    QX_LEAF_CLASS_D, 2048u, nullptr, id), QX_OK);
    ASSERT_EQ(qx_memloc_deallocate(memloc, id, nullptr), QX_OK);

    QXSize used = 0u;
    ASSERT_EQ(qx_memloc_total_used_bytes(memloc, &used), QX_OK);
    EXPECT_EQ(used, 0u);
}

TEST_F(QXMemlocTest, DeallocNullIdReturnsError) {
    EXPECT_NE(qx_memloc_deallocate(memloc, nullptr, nullptr), QX_OK);
}

TEST_F(QXMemlocTest, DoubleDeallocRejected) {
    char id[37]{};
    ASSERT_EQ(alloc("double.free", QX_SEGMENT_CORE,
                    QX_LEAF_CLASS_D, 1024u, nullptr, id), QX_OK);
    ASSERT_EQ(qx_memloc_deallocate(memloc, id, nullptr), QX_OK);
    EXPECT_NE(qx_memloc_deallocate(memloc, id, nullptr), QX_OK);
}

TEST_F(QXMemlocTest, DeallocUnknownIdReturnsError) {
    EXPECT_NE(qx_memloc_deallocate(
        memloc, "00000000-0000-0000-0000-000000000000", nullptr), QX_OK);
}

TEST_F(QXMemlocTest, DeallocReportsBytesFreed) {
    char id[37]{};
    ASSERT_EQ(alloc("freed.bytes", QX_SEGMENT_CORE,
                    QX_LEAF_CLASS_D, 8192u, nullptr, id), QX_OK);
    QXSize freed = 0u;
    ASSERT_EQ(qx_memloc_deallocate(memloc, id, &freed), QX_OK);
    EXPECT_EQ(freed, 8192u);
}

// ---------------------------------------------------------------------------
// MARK: – Touch
// ---------------------------------------------------------------------------

TEST_F(QXMemlocTest, TouchSucceedsOnActiveLeaf) {
    char id[37]{};
    ASSERT_EQ(alloc("touch.leaf", QX_SEGMENT_CORE,
                    QX_LEAF_CLASS_D, 512u, nullptr, id), QX_OK);
    EXPECT_EQ(qx_memloc_touch(memloc, id), QX_OK);
}

TEST_F(QXMemlocTest, TouchFailsOnUnknownId) {
    EXPECT_NE(qx_memloc_touch(
        memloc, "00000000-0000-0000-0000-000000000000"), QX_OK);
}

TEST_F(QXMemlocTest, TouchFailsOnReleasedLeaf) {
    char id[37]{};
    ASSERT_EQ(alloc("touch.released", QX_SEGMENT_CORE,
                    QX_LEAF_CLASS_D, 512u, nullptr, id), QX_OK);
    ASSERT_EQ(qx_memloc_deallocate(memloc, id, nullptr), QX_OK);
    EXPECT_NE(qx_memloc_touch(memloc, id), QX_OK);
}

// ---------------------------------------------------------------------------
// MARK: – Segment access
// ---------------------------------------------------------------------------

TEST_F(QXMemlocTest, GetSegmentSucceedsForValidId) {
    QXSegmentHandle seg = nullptr;
    EXPECT_EQ(qx_memloc_get_segment(memloc, QX_SEGMENT_CORE, &seg), QX_OK);
    EXPECT_NE(seg, nullptr);
}

TEST_F(QXMemlocTest, GetSegmentFailsForUnknownId) {
    QXSegmentHandle seg = nullptr;
    EXPECT_NE(qx_memloc_get_segment(memloc, "QLM-UNKNOWN", &seg), QX_OK);
}

TEST_F(QXMemlocTest, AllNineSegmentsAccessible) {
    for (uint32_t i = 0u; i < QX_SEGMENT_COUNT; ++i) {
        QXSegmentHandle seg = nullptr;
        EXPECT_EQ(qx_memloc_get_segment(memloc, kSegIds[i], &seg), QX_OK)
            << "Failed for: " << kSegIds[i];
        EXPECT_NE(seg, nullptr) << "Null handle for: " << kSegIds[i];
    }
}

// ---------------------------------------------------------------------------
// MARK: – Stats
// ---------------------------------------------------------------------------

TEST_F(QXMemlocTest, TotalUsedBytesZeroOnInit) {
    QXSize used = 99u;
    ASSERT_EQ(qx_memloc_total_used_bytes(memloc, &used), QX_OK);
    EXPECT_EQ(used, 0u);
}

TEST_F(QXMemlocTest, AllStatsReflectAllocations) {
    ASSERT_EQ(alloc("stats.a", QX_SEGMENT_CORE,
                    QX_LEAF_CLASS_D, 1024u), QX_OK);
    ASSERT_EQ(alloc("stats.b", QX_SEGMENT_UI,
                    QX_LEAF_CLASS_D, 2048u), QX_OK);

    QXSegmentStats stats[QX_SEGMENT_COUNT]{};
    ASSERT_EQ(qx_memloc_all_stats(memloc, stats, QX_SEGMENT_COUNT), QX_OK);

    uint64_t total_allocs = 0u;
    for (uint32_t i = 0u; i < QX_SEGMENT_COUNT; ++i)
        total_allocs += stats[i].total_allocations;
    EXPECT_EQ(total_allocs, 2u);
}

TEST_F(QXMemlocTest, SegmentStatsCorrectForCore) {
    ASSERT_EQ(alloc("seg.stats", QX_SEGMENT_CORE,
                    QX_LEAF_CLASS_D, 4096u), QX_OK);

    QXSegmentStats stats{};
    ASSERT_EQ(qx_memloc_segment_stats(memloc, QX_SEGMENT_CORE, &stats), QX_OK);
    EXPECT_EQ(stats.used_bytes,       4096u);
    EXPECT_EQ(stats.active_leaves,    1u);
    EXPECT_EQ(stats.total_allocations,1u);
}

// ---------------------------------------------------------------------------
// MARK: – Multi-segment
// ---------------------------------------------------------------------------

TEST_F(QXMemlocTest, AllocsAcrossSegmentsAccumulateTotal) {
    ASSERT_EQ(alloc("seg.core", QX_SEGMENT_CORE,
                    QX_LEAF_CLASS_D, 1024u), QX_OK);
    ASSERT_EQ(alloc("seg.ui",   QX_SEGMENT_UI,
                    QX_LEAF_CLASS_D, 2048u), QX_OK);
    ASSERT_EQ(alloc("seg.img",  QX_SEGMENT_IMG,
                    QX_LEAF_CLASS_D, 4096u), QX_OK);

    QXSize used = 0u;
    ASSERT_EQ(qx_memloc_total_used_bytes(memloc, &used), QX_OK);
    EXPECT_EQ(used, 1024u + 2048u + 4096u);
}

TEST_F(QXMemlocTest, DeallocFromCorrectSegmentUpdatesThatSegment) {
    char id[37]{};
    ASSERT_EQ(alloc("seg.free.test", QX_SEGMENT_NET,
                    QX_LEAF_CLASS_D, 2048u, nullptr, id), QX_OK);
    ASSERT_EQ(qx_memloc_deallocate(memloc, id, nullptr), QX_OK);

    QXSegmentHandle seg = nullptr;
    ASSERT_EQ(qx_memloc_get_segment(memloc, QX_SEGMENT_NET, &seg), QX_OK);
    QXSize used = 0u;
    ASSERT_EQ(qx_segment_used_bytes(seg, &used), QX_OK);
    EXPECT_EQ(used, 0u);
}

TEST_F(QXMemlocTest, ClassANeverEvictableAcrossAllTiers) {
    QXLeafHandle lf = nullptr;
    ASSERT_EQ(alloc("class.a.leaf", QX_SEGMENT_CORE,
                    QX_LEAF_CLASS_A, 512u, &lf), QX_OK);
    EXPECT_EQ(qx_leaf_evictable_at_tier(lf, QX_TIER_ELEVATED), QX_FALSE);
    EXPECT_EQ(qx_leaf_evictable_at_tier(lf, QX_TIER_HIGH),     QX_FALSE);
    EXPECT_EQ(qx_leaf_evictable_at_tier(lf, QX_TIER_CRITICAL), QX_FALSE);
}

// ---------------------------------------------------------------------------
// MARK: – Equilibrium
// ---------------------------------------------------------------------------

TEST_F(QXMemlocTest, CheckEquilibriumReturnsOkWhenEmpty) {
    QXEquilibriumResult result{};
    ASSERT_EQ(qx_memloc_check_equilibrium(memloc, &result), QX_OK);
}

TEST_F(QXMemlocTest, CheckEquilibriumNullOutReturnsError) {
    EXPECT_NE(qx_memloc_check_equilibrium(memloc, nullptr), QX_OK);
}
