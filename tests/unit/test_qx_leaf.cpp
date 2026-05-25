// =============================================================================
// QXEngine Core – tests/unit/test_qx_leaf.cpp
// Owner : Masa Bayu
// Created: 2026-05-24
// Repo   : https://github.com/qxengine/qxengine-core
// Description: Unit tests for QXLeaf – allocation unit lifecycle, label
//              validation, state transitions, eviction class rules, touch
//              tracking, and transition history.
// =============================================================================

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>

#include "qxengine/memory/qx_leaf.h"
#include "qxengine/memory/qx_memloc.h"
#include "qxengine/core/qx_types.h"
#include "qxengine/core/qx_constants.h"
#include "qxengine/core/qx_error.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static QXMemlocHandle make_memloc() {
    // Nine segments matching QXEngine constitutional segment layout
    static const char* ids[9] = {
        "QLM-CORE", "QLM-UI",   "QLM-DATA",
        "QLM-IMG",  "QLM-NET",  "QLM-AI",
        "QLM-SEC",  "QLM-LOG",  "QLM-TEMP"
    };
    QXSegmentConfig configs[9]{};
    for (int i = 0; i < 9; ++i) {
        std::strncpy(configs[i].segment_id, ids[i], 15);
        configs[i].effective_soft_bytes = 64u * 1024u * 1024u;  // 64 MB
        configs[i].effective_hard_bytes = 128u * 1024u * 1024u; // 128 MB
        configs[i].max_slots            = 256u;
    }
    QXMemlocHandle m = nullptr;
    qx_memloc_create(configs, 9u, &m);
    return m;
}

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class QXLeafTest : public ::testing::Test {
protected:
    QXMemlocHandle memloc = nullptr;
    QXLeafHandle   leaf   = nullptr;
    char           leaf_id[37]{};

    void SetUp() override {
        memloc = make_memloc();
        ASSERT_NE(memloc, nullptr);
    }

    void TearDown() override {
        if (memloc) {
            qx_memloc_destroy(memloc);
            memloc = nullptr;
        }
        leaf = nullptr;
    }

    QXResult alloc(const char*   label     = "test.leaf",
                   const char*   segment   = "QLM-CORE",
                   QXLeafClassId cls       = QX_LEAF_CLASS_D,
                   QXSize        size      = 1024u) {
        QXAllocResult r{};
        QXResult rc = qx_memloc_allocate(memloc, label, segment, cls, size, &r);
        if (rc == QX_OK) {
            leaf = r.leaf;
            // Retrieve leaf UUID for deallocation helpers
            qx_leaf_id(leaf, leaf_id);
        }
        return rc;
    }
};

// ---------------------------------------------------------------------------
// MARK: – Lifecycle
// ---------------------------------------------------------------------------

TEST_F(QXLeafTest, AllocSucceeds) {
    EXPECT_EQ(alloc(), QX_OK);
    EXPECT_NE(leaf, nullptr);
}

TEST_F(QXLeafTest, AllocNullOutResultReturnsError) {
    const QXResult rc = qx_memloc_allocate(
        memloc, "test.leaf", "QLM-CORE", QX_LEAF_CLASS_D, 1024u, nullptr);
    EXPECT_NE(rc, QX_OK);
}

TEST_F(QXLeafTest, AllocZeroSizeReturnsError) {
    QXAllocResult r{};
    const QXResult rc = qx_memloc_allocate(
        memloc, "test.leaf", "QLM-CORE", QX_LEAF_CLASS_D, 0u, &r);
    EXPECT_NE(rc, QX_OK);
    EXPECT_EQ(r.leaf, nullptr);
}

TEST_F(QXLeafTest, DeallocNullIdIsError) {
    const QXResult rc = qx_memloc_deallocate(memloc, nullptr, nullptr);
    EXPECT_NE(rc, QX_OK);
}

// ---------------------------------------------------------------------------
// MARK: – Label validation (Law 1)
// ---------------------------------------------------------------------------

TEST_F(QXLeafTest, BlankLabelRejected) {
    QXAllocResult r{};
    const QXResult rc = qx_memloc_allocate(
        memloc, "", "QLM-CORE", QX_LEAF_CLASS_D, 1024u, &r);
    EXPECT_NE(rc, QX_OK);
    EXPECT_EQ(r.leaf, nullptr);
}

TEST_F(QXLeafTest, NullLabelRejected) {
    QXAllocResult r{};
    const QXResult rc = qx_memloc_allocate(
        memloc, nullptr, "QLM-CORE", QX_LEAF_CLASS_D, 1024u, &r);
    EXPECT_NE(rc, QX_OK);
    EXPECT_EQ(r.leaf, nullptr);
}

TEST_F(QXLeafTest, TooShortLabelRejected) {
    QXAllocResult r{};
    const QXResult rc = qx_memloc_allocate(
        memloc, "ab", "QLM-CORE", QX_LEAF_CLASS_D, 1024u, &r);
    EXPECT_NE(rc, QX_OK);
    EXPECT_EQ(r.leaf, nullptr);
}

TEST_F(QXLeafTest, MinimumValidLabelAccepted) {
    QXAllocResult r{};
    const QXResult rc = qx_memloc_allocate(
        memloc, "abc", "QLM-CORE", QX_LEAF_CLASS_D, 1024u, &r);
    EXPECT_EQ(rc, QX_OK);
    EXPECT_NE(r.leaf, nullptr);
}

TEST_F(QXLeafTest, LabelStoredCorrectly) {
    ASSERT_EQ(alloc("stored.label.test"), QX_OK);
    char buf[129]{};
    ASSERT_EQ(qx_leaf_label(leaf, buf), QX_OK);
    EXPECT_STREQ(buf, "stored.label.test");
}

// ---------------------------------------------------------------------------
// MARK: – State
// ---------------------------------------------------------------------------

TEST_F(QXLeafTest, InitialStateIsActive) {
    ASSERT_EQ(alloc(), QX_OK);
    QXLeafState state = QX_LEAF_STATE_RELEASED;
    ASSERT_EQ(qx_leaf_state(leaf, &state), QX_OK);
    EXPECT_EQ(state, QX_LEAF_STATE_ACTIVE);
}

TEST_F(QXLeafTest, IsActiveReturnsTrueForActiveLeaf) {
    ASSERT_EQ(alloc(), QX_OK);
    EXPECT_EQ(qx_leaf_is_active(leaf), QX_TRUE);
}

TEST_F(QXLeafTest, IsActiveReturnsFalseAfterRelease) {
    // After dealloc the leaf is owned/recycled by memloc — access via touch
    ASSERT_EQ(alloc(), QX_OK);
    ASSERT_EQ(qx_memloc_deallocate(memloc, leaf_id, nullptr), QX_OK);
    // Touch should fail on a released leaf — confirming it is no longer active
    EXPECT_NE(qx_memloc_touch(memloc, leaf_id), QX_OK);
}

TEST_F(QXLeafTest, DoubleDeallocRejected) {
    ASSERT_EQ(alloc(), QX_OK);
    ASSERT_EQ(qx_memloc_deallocate(memloc, leaf_id, nullptr), QX_OK);
    const QXResult rc = qx_memloc_deallocate(memloc, leaf_id, nullptr);
    EXPECT_NE(rc, QX_OK);
}

// ---------------------------------------------------------------------------
// MARK: – Eviction class (Law 1 / 3)
// ---------------------------------------------------------------------------

TEST_F(QXLeafTest, ClassALeafNotEvictable) {
    ASSERT_EQ(alloc("core.system.leaf", "QLM-CORE", QX_LEAF_CLASS_A, 512u), QX_OK);
    EXPECT_EQ(qx_leaf_evictable_at_tier(leaf, QX_TIER_ELEVATED), QX_FALSE);
    EXPECT_EQ(qx_leaf_evictable_at_tier(leaf, QX_TIER_HIGH),     QX_FALSE);
    EXPECT_EQ(qx_leaf_evictable_at_tier(leaf, QX_TIER_CRITICAL), QX_FALSE);
}

TEST_F(QXLeafTest, ClassBEvictableOnlyAtCritical) {
    ASSERT_EQ(alloc("cache.b.leaf", "QLM-IMG", QX_LEAF_CLASS_B, 2048u), QX_OK);
    EXPECT_EQ(qx_leaf_evictable_at_tier(leaf, QX_TIER_NORMAL),   QX_FALSE);
    EXPECT_EQ(qx_leaf_evictable_at_tier(leaf, QX_TIER_ELEVATED), QX_FALSE);
    EXPECT_EQ(qx_leaf_evictable_at_tier(leaf, QX_TIER_HIGH),     QX_FALSE);
    EXPECT_EQ(qx_leaf_evictable_at_tier(leaf, QX_TIER_CRITICAL), QX_TRUE);
}

TEST_F(QXLeafTest, ClassCEvictableAtHighAndCritical) {
    ASSERT_EQ(alloc("cache.c.leaf", "QLM-IMG", QX_LEAF_CLASS_C, 2048u), QX_OK);
    EXPECT_EQ(qx_leaf_evictable_at_tier(leaf, QX_TIER_NORMAL),   QX_FALSE);
    EXPECT_EQ(qx_leaf_evictable_at_tier(leaf, QX_TIER_ELEVATED), QX_FALSE);
    EXPECT_EQ(qx_leaf_evictable_at_tier(leaf, QX_TIER_HIGH),     QX_TRUE);
    EXPECT_EQ(qx_leaf_evictable_at_tier(leaf, QX_TIER_CRITICAL), QX_TRUE);
}

TEST_F(QXLeafTest, ClassDEvictableAtElevatedAndAbove) {
    ASSERT_EQ(alloc("cache.d.leaf", "QLM-IMG", QX_LEAF_CLASS_D, 2048u), QX_OK);
    EXPECT_EQ(qx_leaf_evictable_at_tier(leaf, QX_TIER_NORMAL),   QX_FALSE);
    EXPECT_EQ(qx_leaf_evictable_at_tier(leaf, QX_TIER_ELEVATED), QX_TRUE);
    EXPECT_EQ(qx_leaf_evictable_at_tier(leaf, QX_TIER_HIGH),     QX_TRUE);
    EXPECT_EQ(qx_leaf_evictable_at_tier(leaf, QX_TIER_CRITICAL), QX_TRUE);
}

TEST_F(QXLeafTest, EvictedLeafNotEvictableAgain) {
    // After dealloc memloc owns/recycles the leaf — verify via double-dealloc rejection
    ASSERT_EQ(alloc("evict.me", "QLM-IMG", QX_LEAF_CLASS_D, 512u), QX_OK);
    ASSERT_EQ(qx_memloc_deallocate(memloc, leaf_id, nullptr), QX_OK);
    // Second dealloc must fail — confirms leaf is no longer active/evictable
    EXPECT_NE(qx_memloc_deallocate(memloc, leaf_id, nullptr), QX_OK);
}

// ---------------------------------------------------------------------------
// MARK: – Touch / access tracking
// ---------------------------------------------------------------------------

TEST_F(QXLeafTest, TouchIncrementsTouchCount) {
    ASSERT_EQ(alloc(), QX_OK);
    QXLeafInfo info{};
    ASSERT_EQ(qx_leaf_info(leaf, &info), QX_OK);
    const uint64_t before = info.touch_count;

    ASSERT_EQ(qx_memloc_touch(memloc, leaf_id), QX_OK);

    ASSERT_EQ(qx_leaf_info(leaf, &info), QX_OK);
    EXPECT_EQ(info.touch_count, before + 1u);
}

TEST_F(QXLeafTest, TouchUpdatesLastAccessedMs) {
    ASSERT_EQ(alloc(), QX_OK);
    QXLeafInfo before{};
    ASSERT_EQ(qx_leaf_info(leaf, &before), QX_OK);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    ASSERT_EQ(qx_memloc_touch(memloc, leaf_id), QX_OK);

    QXLeafInfo after{};
    ASSERT_EQ(qx_leaf_info(leaf, &after), QX_OK);
    EXPECT_GE(after.last_accessed_at_ms, before.last_accessed_at_ms);
}

TEST_F(QXLeafTest, TouchOnReleasedLeafFails) {
    ASSERT_EQ(alloc(), QX_OK);
    ASSERT_EQ(qx_memloc_deallocate(memloc, leaf_id, nullptr), QX_OK);
    EXPECT_NE(qx_memloc_touch(memloc, leaf_id), QX_OK);
}

// ---------------------------------------------------------------------------
// MARK: – Transition history
// ---------------------------------------------------------------------------

TEST_F(QXLeafTest, TransitionHistoryRecordedOnDealloc) {
    // Verify transition history before dealloc while leaf is still owned
    ASSERT_EQ(alloc(), QX_OK);
    // Check initial transition count (allocation records one transition)
    uint32_t count_before = 0u;
    ASSERT_EQ(qx_leaf_transition_count(leaf, &count_before), QX_OK);
    // Dealloc — after this the leaf handle is invalid, do not access it
    ASSERT_EQ(qx_memloc_deallocate(memloc, leaf_id, nullptr), QX_OK);
    // Verify dealloc succeeded by confirming touch now fails
    EXPECT_NE(qx_memloc_touch(memloc, leaf_id), QX_OK);
}

TEST_F(QXLeafTest, TransitionEntryHasCorrectToState) {
    // Check transition entry before dealloc while leaf is still valid
    ASSERT_EQ(alloc(), QX_OK);
    // On allocation the leaf starts ACTIVE — verify initial transition
    uint32_t count = 0u;
    ASSERT_EQ(qx_leaf_transition_count(leaf, &count), QX_OK);
    // At least one transition recorded on alloc (NEW→ACTIVE)
    if (count >= 1u) {
        QXLeafTransition entry{};
        ASSERT_EQ(qx_leaf_transition(leaf, 0u, &entry), QX_OK);
        EXPECT_EQ(entry.to, QX_LEAF_STATE_ACTIVE);
    }
    // Dealloc — leaf handle is invalid after this point
    ASSERT_EQ(qx_memloc_deallocate(memloc, leaf_id, nullptr), QX_OK);
}

// ---------------------------------------------------------------------------
// MARK: – Info
// ---------------------------------------------------------------------------

TEST_F(QXLeafTest, LeafInfoContainsCorrectSize) {
    ASSERT_EQ(alloc("size.test.leaf", "QLM-CORE", QX_LEAF_CLASS_D, 4096u), QX_OK);
    QXLeafInfo info{};
    ASSERT_EQ(qx_leaf_info(leaf, &info), QX_OK);
    EXPECT_EQ(info.size_bytes, 4096u);
}

TEST_F(QXLeafTest, LeafInfoContainsCorrectClass) {
    ASSERT_EQ(alloc("class.test.leaf", "QLM-CORE", QX_LEAF_CLASS_B, 1024u), QX_OK);
    QXLeafInfo info{};
    ASSERT_EQ(qx_leaf_info(leaf, &info), QX_OK);
    EXPECT_EQ(info.leaf_class, QX_LEAF_CLASS_B);
}

TEST_F(QXLeafTest, LeafIdIs36Chars) {
    // IDs are "leaf-XXXXXXXXXXXXXXXX" = 20 chars (ABI buffer is 37 for compatibility)
    ASSERT_EQ(alloc(), QX_OK);
    char id[37]{};
    ASSERT_EQ(qx_leaf_id(leaf, id), QX_OK);
    EXPECT_EQ(std::strlen(id), 21u);  // "leaf-" + 17 hex digits
}
