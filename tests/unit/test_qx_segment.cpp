// =============================================================================
// QXEngine Core – tests/unit/test_qx_segment.cpp
// Owner : Masa Bayu
// Created: 2026-05-24
// Repo   : https://github.com/qxengine/qxengine-core
// Description: Unit tests for QXSegment – lifecycle, allocation, deallocation,
//              eviction, stats, scalar accessors, predicates, and eviction
//              candidate collection.
// =============================================================================

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <cstdio>

#include "qxengine/memory/qx_segment.h"
#include "qxengine/memory/qx_leaf.h"
#include "qxengine/memory/qx_memloc.h"
#include "qxengine/core/qx_types.h"
#include "qxengine/core/qx_constants.h"
#include "qxengine/core/qx_error.h"

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr QXSize   kSoftLimit = 64u  * 1024u * 1024u;
static constexpr QXSize   kHardLimit = 128u * 1024u * 1024u;
static constexpr uint32_t kMaxSlots  = 7u;
static const char*        kSegId     = "QLM-CORE";

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class QXSegmentTest : public ::testing::Test {
protected:
    QXMemlocHandle  memloc  = nullptr;
    QXSegmentHandle segment = nullptr;

    void SetUp() override {
        // Build a memloc with one segment under test plus eight others
        static const char* ids[9] = {
            "QLM-CORE", "QLM-UI",   "QLM-DATA",
            "QLM-IMG",  "QLM-NET",  "QLM-AI",
            "QLM-SEC",  "QLM-LOG",  "QLM-TEMP"
        };
        QXSegmentConfig configs[9]{};
        for (int i = 0; i < 9; ++i) {
            std::strncpy(configs[i].segment_id, ids[i], 15);
            configs[i].effective_soft_bytes = kSoftLimit;
            configs[i].effective_hard_bytes = kHardLimit;
            configs[i].max_slots            = kMaxSlots;
        }
        ASSERT_EQ(qx_memloc_create(configs, 9u, &memloc), QX_OK);
        ASSERT_NE(memloc, nullptr);

        // Retrieve the QLM-TEST segment handle
        ASSERT_EQ(qx_memloc_get_segment(memloc, kSegId, &segment), QX_OK);
        ASSERT_NE(segment, nullptr);
    }

    void TearDown() override {
        if (memloc) {
            qx_memloc_destroy(memloc);
            memloc  = nullptr;
            segment = nullptr;
        }
    }

    // Allocate a leaf into QLM-TEST and return handle + uuid
    QXResult alloc(const char*   label,
                   QXLeafClassId cls,
                   QXSize        size,
                   QXLeafHandle* out_leaf,
                   char          out_id[37] = nullptr) {
        QXAllocResult r{};
        QXResult rc = qx_memloc_allocate(memloc, label, kSegId, cls, size, &r);
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

TEST_F(QXSegmentTest, SegmentHandleValid) {
    EXPECT_NE(segment, nullptr);
}

TEST_F(QXSegmentTest, IdAccessorReturnsCorrectId) {
    char buf[16]{};
    ASSERT_EQ(qx_segment_id(segment, buf), QX_OK);
    EXPECT_STREQ(buf, kSegId);
}

TEST_F(QXSegmentTest, SoftLimitAccessorCorrect) {
    QXSize soft = 0u;
    ASSERT_EQ(qx_segment_soft_limit(segment, &soft), QX_OK);
    EXPECT_EQ(soft, kSoftLimit);
}

TEST_F(QXSegmentTest, HardLimitAccessorCorrect) {
    QXSize hard = 0u;
    ASSERT_EQ(qx_segment_hard_limit(segment, &hard), QX_OK);
    EXPECT_EQ(hard, kHardLimit);
}

// ---------------------------------------------------------------------------
// MARK: – Allocation
// ---------------------------------------------------------------------------

TEST_F(QXSegmentTest, AllocateLeafSucceeds) {
    QXLeafHandle lf = nullptr;
    EXPECT_EQ(alloc("alloc.test", QX_LEAF_CLASS_D, 1024u, &lf), QX_OK);
    EXPECT_NE(lf, nullptr);
}

TEST_F(QXSegmentTest, AllocateUpdatesUsedBytes) {
    QXLeafHandle lf = nullptr;
    ASSERT_EQ(alloc("bytes.test", QX_LEAF_CLASS_D, 4096u, &lf), QX_OK);

    QXSize used = 0u;
    ASSERT_EQ(qx_segment_used_bytes(segment, &used), QX_OK);
    EXPECT_EQ(used, 4096u);
}

TEST_F(QXSegmentTest, AllocateOverHardLimitRejected) {
    QXAllocResult r{};
    const QXResult rc = qx_memloc_allocate(
        memloc, "over.hard.limit", kSegId,
        QX_LEAF_CLASS_D, kHardLimit + 1u, &r);
    EXPECT_NE(rc, QX_OK);
    EXPECT_EQ(r.leaf, nullptr);
}

TEST_F(QXSegmentTest, AllocateFillsAllSlots) {
    for (uint32_t i = 0u; i < kMaxSlots; ++i) {
        char label[32];
        std::snprintf(label, sizeof(label), "slot.leaf.%u", i);
        QXLeafHandle lf = nullptr;
        ASSERT_EQ(alloc(label, QX_LEAF_CLASS_D, 1024u, &lf), QX_OK);
    }
    QXAllocResult r{};
    const QXResult rc = qx_memloc_allocate(
        memloc, "extra.leaf", kSegId, QX_LEAF_CLASS_D, 1024u, &r);
    EXPECT_NE(rc, QX_OK);
    EXPECT_EQ(r.leaf, nullptr);
}

// ---------------------------------------------------------------------------
// MARK: – Deallocation
// ---------------------------------------------------------------------------

TEST_F(QXSegmentTest, DeallocateReducesUsedBytes) {
    char id[37]{};
    QXLeafHandle lf = nullptr;
    ASSERT_EQ(alloc("dealloc.test", QX_LEAF_CLASS_D, 2048u, &lf, id), QX_OK);

    QXSize freed = 0u;
    ASSERT_EQ(qx_memloc_deallocate(memloc, id, &freed), QX_OK);
    EXPECT_EQ(freed, 2048u);

    QXSize used = 0u;
    ASSERT_EQ(qx_segment_used_bytes(segment, &used), QX_OK);
    EXPECT_EQ(used, 0u);
}

TEST_F(QXSegmentTest, DeallocateFreesSlotForReuse) {
    char id[37]{};
    QXLeafHandle lf = nullptr;
    ASSERT_EQ(alloc("reuse.leaf", QX_LEAF_CLASS_D, 1024u, &lf, id), QX_OK);
    ASSERT_EQ(qx_memloc_deallocate(memloc, id, nullptr), QX_OK);

    QXLeafHandle lf2 = nullptr;
    EXPECT_EQ(alloc("reuse.leaf2", QX_LEAF_CLASS_D, 1024u, &lf2), QX_OK);
}

TEST_F(QXSegmentTest, DoubleDeallocRejected) {
    char id[37]{};
    QXLeafHandle lf = nullptr;
    ASSERT_EQ(alloc("dbl.dealloc", QX_LEAF_CLASS_D, 1024u, &lf, id), QX_OK);
    ASSERT_EQ(qx_memloc_deallocate(memloc, id, nullptr), QX_OK);
    EXPECT_NE(qx_memloc_deallocate(memloc, id, nullptr), QX_OK);
}

TEST_F(QXSegmentTest, DeallocateUnknownIdReturnsError) {
    const QXResult rc = qx_memloc_deallocate(
        memloc, "00000000-0000-0000-0000-000000000000", nullptr);
    EXPECT_NE(rc, QX_OK);
}

// ---------------------------------------------------------------------------
// MARK: – Stats
// ---------------------------------------------------------------------------

TEST_F(QXSegmentTest, StatsReflectAllocations) {
    QXLeafHandle lf = nullptr;
    ASSERT_EQ(alloc("stats.leaf", QX_LEAF_CLASS_D, 1024u, &lf), QX_OK);

    QXSegmentStats stats{};
    ASSERT_EQ(qx_segment_stats(segment, &stats), QX_OK);
    EXPECT_EQ(stats.total_allocations, 1u);
    EXPECT_EQ(stats.active_leaves,     1u);
    EXPECT_EQ(stats.used_bytes,        1024u);
}

TEST_F(QXSegmentTest, StatsPairsRatioIsOneWhenEmpty) {
    QXSegmentStats stats{};
    ASSERT_EQ(qx_segment_stats(segment, &stats), QX_OK);
    EXPECT_DOUBLE_EQ(stats.pairs_ratio, 1.0);
}

TEST_F(QXSegmentTest, StatsPressureTierNormalWhenEmpty) {
    QXSegmentStats stats{};
    ASSERT_EQ(qx_segment_stats(segment, &stats), QX_OK);
    EXPECT_EQ(stats.pressure_tier, QX_TIER_NORMAL);
}

TEST_F(QXSegmentTest, StatsReflectDeallocations) {
    char id[37]{};
    QXLeafHandle lf = nullptr;
    ASSERT_EQ(alloc("dealloc.stats", QX_LEAF_CLASS_D, 1024u, &lf, id), QX_OK);
    ASSERT_EQ(qx_memloc_deallocate(memloc, id, nullptr), QX_OK);

    QXSegmentStats stats{};
    ASSERT_EQ(qx_segment_stats(segment, &stats), QX_OK);
    EXPECT_EQ(stats.total_deallocations, 1u);
    EXPECT_EQ(stats.active_leaves,       0u);
    EXPECT_EQ(stats.used_bytes,          0u);
}

// ---------------------------------------------------------------------------
// MARK: – Scalar accessors
// ---------------------------------------------------------------------------

TEST_F(QXSegmentTest, PressureTierNormalWhenIdle) {
    QXTierId tier = QX_TIER_CRITICAL;
    ASSERT_EQ(qx_segment_pressure_tier(segment, &tier), QX_OK);
    EXPECT_EQ(tier, QX_TIER_NORMAL);
}

TEST_F(QXSegmentTest, PairsRatioOneWhenEmpty) {
    double ratio = 0.0;
    ASSERT_EQ(qx_segment_pairs_ratio(segment, &ratio), QX_OK);
    EXPECT_DOUBLE_EQ(ratio, 1.0);
}

TEST_F(QXSegmentTest, OrphanedBytesZeroWhenFresh) {
    QXSize orphaned = 99u;
    ASSERT_EQ(qx_segment_orphaned_bytes(segment, &orphaned), QX_OK);
    EXPECT_EQ(orphaned, 0u);
}

// ---------------------------------------------------------------------------
// MARK: – Predicates
// ---------------------------------------------------------------------------

TEST_F(QXSegmentTest, HasCapacityTrueWhenEmpty) {
    EXPECT_EQ(qx_segment_has_capacity(segment), QX_TRUE);
}

TEST_F(QXSegmentTest, HasCapacityFalseWhenFull) {
    for (uint32_t i = 0u; i < kMaxSlots; ++i) {
        char label[32];
        std::snprintf(label, sizeof(label), "cap.leaf.%u", i);
        QXLeafHandle lf = nullptr;
        ASSERT_EQ(alloc(label, QX_LEAF_CLASS_D, 1024u, &lf), QX_OK);
    }
    EXPECT_EQ(qx_segment_has_capacity(segment), QX_FALSE);
}

TEST_F(QXSegmentTest, OverSoftLimitFalseWhenEmpty) {
    EXPECT_EQ(qx_segment_over_soft_limit(segment), QX_FALSE);
}

TEST_F(QXSegmentTest, OverHardLimitFalseWhenEmpty) {
    EXPECT_EQ(qx_segment_over_hard_limit(segment), QX_FALSE);
}

// ---------------------------------------------------------------------------
// MARK: – Eviction candidates
// ---------------------------------------------------------------------------

TEST_F(QXSegmentTest, EvictionCandidateCountZeroForClassA) {
    QXLeafHandle lf = nullptr;
    ASSERT_EQ(alloc("protected.leaf", QX_LEAF_CLASS_A, 1024u, &lf), QX_OK);

    uint32_t count = 99u;
    ASSERT_EQ(qx_segment_eviction_candidate_count(
        segment, QX_TIER_CRITICAL, &count), QX_OK);
    EXPECT_EQ(count, 0u);
}

TEST_F(QXSegmentTest, EvictionCandidateCountNonZeroForClassDAtElevated) {
    QXLeafHandle lf = nullptr;
    ASSERT_EQ(alloc("evictable.leaf", QX_LEAF_CLASS_D, 1024u, &lf), QX_OK);

    uint32_t count = 0u;
    ASSERT_EQ(qx_segment_eviction_candidate_count(
        segment, QX_TIER_ELEVATED, &count), QX_OK);
    EXPECT_GE(count, 1u);
}

TEST_F(QXSegmentTest, LeafAtSlotNullForEmptySlot) {
    QXLeafHandle lf = reinterpret_cast<QXLeafHandle>(0xDEAD);
    const QXResult rc = qx_segment_leaf_at_slot(segment, 0u, &lf);
    // Either returns error or sets lf to nullptr for an empty slot
    if (rc == QX_OK) EXPECT_EQ(lf, nullptr);
}

TEST_F(QXSegmentTest, FlushTerminatedReturnsOk) {
    uint32_t flushed = 99u;
    ASSERT_EQ(qx_segment_flush_terminated(segment, &flushed), QX_OK);
    // Nothing to flush on a fresh segment
    EXPECT_EQ(flushed, 0u);
}
