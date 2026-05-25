/* ============================================================
 * test_qx_memloc_constitutional.cpp
 * QXMemloc Constitutional Allocator — Test Suite
 *
 * Tests verify constitutional correctness, not just
 * functional correctness. Every test maps to a specific
 * law of Z or X, an ABA phase, or the Universal Formula.
 *
 * ALAMTOLOGI — Quranic Science
 * Founded by Masa Bayu
 *
 * Owner : Masa Bayu
 * Date  : 2026-05-25
 * ============================================================ */

#include <gtest/gtest.h>
#include "qxengine/memory/qx_memloc_constitutional.h"
/* ── Test fixture ───────────────────────────────────────────── */
class QXMemlocConstitutionalTest : public ::testing::Test {
protected:
    QXMemlocConstitutional memloc{};
    static constexpr QXSize  SOFT_LIMIT  = 512u * 1048576u;
    static constexpr QXSize  HARD_LIMIT  = 768u * 1048576u;
    static constexpr double  DEVICE_SCALE = 1.0;
    static constexpr double  DECLARED_X   = 1.0;
    static constexpr double  X_TOLERANCE  = 15.0;
    void SetUp() override {
        QXResult r = qx_memloc_constitutional_create(
            SOFT_LIMIT, HARD_LIMIT, DEVICE_SCALE,
            DECLARED_X, X_TOLERANCE, &memloc
        );
        ASSERT_EQ(r, QX_OK);
    }
    void TearDown() override {
        qx_memloc_constitutional_destroy(&memloc);
    }
};
/* ── Z.1 Pattern tests ──────────────────────────────────────── */
TEST_F(QXMemlocConstitutionalTest, PatternLawRejectsNullLabel) {
    QXFlowAllocation alloc{};
    QXResult r = qx_flow_allocate(
        memloc.flow, nullptr,
        "QLM-IMG", QX_LEAF_CLASS_C,
        1048576u, &alloc
    );
    EXPECT_EQ(r, QX_ERR_LABEL_BLANK);
    EXPECT_EQ(alloc.ptr, nullptr);
    EXPECT_EQ(alloc.leaf, nullptr);
}
TEST_F(QXMemlocConstitutionalTest, PatternLawRejectsEmptyLabel) {
    QXFlowAllocation alloc{};
    QXResult r = qx_flow_allocate(
        memloc.flow, "",
        "QLM-IMG", QX_LEAF_CLASS_C,
        1048576u, &alloc
    );
    EXPECT_EQ(r, QX_ERR_LABEL_TOO_SHORT);
    EXPECT_EQ(alloc.ptr, nullptr);
}
TEST_F(QXMemlocConstitutionalTest, PatternLawRejectsTooShortLabel) {
    QXFlowAllocation alloc{};
    QXResult r = qx_flow_allocate(
        memloc.flow, "ab",
        "QLM-IMG", QX_LEAF_CLASS_C,
        1048576u, &alloc
    );
    EXPECT_EQ(r, QX_ERR_LABEL_TOO_SHORT);
}
TEST_F(QXMemlocConstitutionalTest, PatternLawAcceptsValidLabel) {
    QXFlowAllocation alloc{};
    QXResult r = qx_flow_allocate(
        memloc.flow, "img.decoded.home.card",
        "QLM-IMG", QX_LEAF_CLASS_C,
        1048576u, &alloc
    );
    EXPECT_EQ(r, QX_OK);
    EXPECT_NE(alloc.ptr, nullptr);
    EXPECT_NE(alloc.leaf, nullptr);

    /* Pairs — clean up */
    QXFlowRelease release{};
    qx_flow_deallocate(memloc.flow, alloc.leaf, &release);
}
TEST_F(QXMemlocConstitutionalTest, PatternLawPtrAndLeafAreInseparable) {
    QXFlowAllocation alloc{};
    QXResult r = qx_flow_allocate(
        memloc.flow, "img.decoded.home.card",
        "QLM-IMG", QX_LEAF_CLASS_C,
        1048576u, &alloc
    );
    ASSERT_EQ(r, QX_OK);

    /* Both must be non-null — they are born together */
    EXPECT_NE(alloc.ptr, nullptr);
    EXPECT_NE(alloc.leaf, nullptr);

    QXFlowRelease release{};
    qx_flow_deallocate(memloc.flow, alloc.leaf, &release);
}
/* ── Z.2 Limit tests ────────────────────────────────────────── */
TEST_F(QXMemlocConstitutionalTest, LimitLawRejectsZeroBytes) {
    QXFlowAllocation alloc{};
    QXResult r = qx_flow_allocate(
        memloc.flow, "img.decoded.zero",
        "QLM-IMG", QX_LEAF_CLASS_C,
        0u, &alloc
    );
    EXPECT_EQ(r, QX_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(alloc.ptr, nullptr);
}
TEST_F(QXMemlocConstitutionalTest, LimitLawRejectsExceedingHardBudget) {
    QXFlowAllocation alloc{};
    QXResult r = qx_flow_allocate(
        memloc.flow, "img.decoded.overflow",
        "QLM-IMG", QX_LEAF_CLASS_C,
        HARD_LIMIT + 1u, &alloc
    );
    EXPECT_EQ(r, QX_ERR_HARD_BUDGET);
    EXPECT_EQ(alloc.ptr, nullptr);
}

TEST_F(QXMemlocConstitutionalTest, LimitLawTracksPressurePctAfterAlloc) {
    QXFlowAllocation alloc{};
    QXResult r = qx_flow_allocate(
        memloc.flow, "img.decoded.pressure.check",
        "QLM-IMG", QX_LEAF_CLASS_C,
        64u * 1048576u, &alloc
    );
    ASSERT_EQ(r, QX_OK);

    /* Pressure pct must be > 0 after allocation */
    EXPECT_GT(alloc.soft_pressure_pct, 0.0);
    EXPECT_LE(alloc.soft_pressure_pct, 100.0);

    QXFlowRelease release{};
    qx_flow_deallocate(memloc.flow, alloc.leaf, &release);
}

/* ── Z.3 Pairs tests ────────────────────────────────────────── */

TEST_F(QXMemlocConstitutionalTest, PairsLawEveryAllocHasDealloc) {
    QXFlowAllocation alloc{};
    ASSERT_EQ(qx_flow_allocate(
        memloc.flow, "net.api.response.pairs",
        "QLM-NET", QX_LEAF_CLASS_C,
        65536u, &alloc
    ), QX_OK);

    ASSERT_NE(alloc.ptr, nullptr);

    QXFlowRelease release{};
    QXResult r = qx_flow_deallocate(
        memloc.flow, alloc.leaf, &release
    );
    EXPECT_EQ(r, QX_OK);

    /* Bytes must be returned */
    EXPECT_GT(release.released_bytes, 0u);
    EXPECT_GT(release.released_at_ms, 0u);
}

TEST_F(QXMemlocConstitutionalTest, PairsLawDoubleFreeIsRejected) {
    QXFlowAllocation alloc{};
    ASSERT_EQ(qx_flow_allocate(
        memloc.flow, "net.api.response.doublefree",
        "QLM-NET", QX_LEAF_CLASS_C,
        65536u, &alloc
    ), QX_OK);

    QXFlowRelease release{};
    ASSERT_EQ(qx_flow_deallocate(
        memloc.flow, alloc.leaf, &release
    ), QX_OK);

    /* Second free must fail */
    QXResult r = qx_flow_deallocate(
        memloc.flow, alloc.leaf, &release
    );
    EXPECT_EQ(r, QX_ERR_DOUBLE_FREE);
}

TEST_F(QXMemlocConstitutionalTest, PairsLawFinalXRecordedOnRelease) {
    QXFlowAllocation alloc{};
    ASSERT_EQ(qx_flow_allocate(
        memloc.flow, "img.decoded.x.record",
        "QLM-IMG", QX_LEAF_CLASS_C,
        1048576u, &alloc
    ), QX_OK);

    QXFlowRelease release{};
    ASSERT_EQ(qx_flow_deallocate(
        memloc.flow, alloc.leaf, &release
    ), QX_OK);

    /* Final X must be recorded */
    EXPECT_GT(release.final_x, 0.0);
}

/* ── Z.4 Equilibrium tests ──────────────────────────────────── */

TEST_F(QXMemlocConstitutionalTest, EquilibriumLawDetectsImbalance) {
    /* Allocate heavily in one segment */
    QXFlowAllocation alloc{};
    ASSERT_EQ(qx_flow_allocate(
        memloc.flow, "img.decoded.equilibrium.heavy",
        "QLM-IMG", QX_LEAF_CLASS_A,
        96u * 1048576u, &alloc
    ), QX_OK);

    QXEnforcementResult result{};
    ASSERT_EQ(qx_enforce_equilibrium(
        memloc.enforce, &result
    ), QX_OK);

    /* Imbalance should be detectable */
    EXPECT_FALSE(result.equilibrium_restored);

    QXFlowRelease release{};
    qx_flow_deallocate(memloc.flow, alloc.leaf, &release);
}

TEST_F(QXMemlocConstitutionalTest, EquilibriumRestoredAfterRelease) {
    QXFlowAllocation alloc{};
    ASSERT_EQ(qx_flow_allocate(
        memloc.flow, "img.decoded.equilibrium.restore",
        "QLM-IMG", QX_LEAF_CLASS_A,
        96u * 1048576u, &alloc
    ), QX_OK);

    QXFlowRelease release{};
    qx_flow_deallocate(memloc.flow, alloc.leaf, &release);

    QXEnforcementResult result{};
    ASSERT_EQ(qx_enforce_equilibrium(
        memloc.enforce, &result
    ), QX_OK);

    EXPECT_TRUE(result.equilibrium_restored);
}

/* ── Universal Formula X tests ──────────────────────────────── */

TEST_F(QXMemlocConstitutionalTest, UniversalFormulaXMeasuredEachCycle) {
    QXFlowAllocation alloc{};
    ASSERT_EQ(qx_flow_allocate(
        memloc.flow, "img.decoded.x.measure",
        "QLM-IMG", QX_LEAF_CLASS_C,
        4u * 1048576u, &alloc
    ), QX_OK);

    QXEngineABAMeasurement  engine_m{};
    QXSegmentABAMeasurement seg_m[QX_SEGMENT_COUNT]{};

    QXResult r = qx_signal_measure_cycle(
        memloc.signal,
        (QXTimestamp)(alloc.allocated_at_ms + 5000u),
        &engine_m,
        seg_m,
        QX_SEGMENT_COUNT
    );
    EXPECT_EQ(r, QX_OK);
    EXPECT_GT(engine_m.measured_x, 0.0);
    EXPECT_GT(engine_m.total_leaves_measured, 0u);

    QXFlowRelease release{};
    qx_flow_deallocate(memloc.flow, alloc.leaf, &release);
}

TEST_F(QXMemlocConstitutionalTest, UniversalFormulaFatalDriftEvicts) {
    QXFlowAllocation alloc{};
    ASSERT_EQ(qx_flow_allocate(
        memloc.flow, "img.decoded.x.fatal",
        "QLM-IMG", QX_LEAF_CLASS_D,
        1u * 1048576u, &alloc
    ), QX_OK);

    /* Simulate fatal drift by measuring with extreme time delta */
    QXEngineABAMeasurement engine_m{};
    QXSegmentABAMeasurement seg_m[QX_SEGMENT_COUNT]{};

    qx_signal_measure_cycle(
        memloc.signal,
        alloc.allocated_at_ms + 999999u, /* extreme time delta */
        &engine_m, seg_m, QX_SEGMENT_COUNT
    );

    QXEnforcementResult enforce_r{};
    qx_enforce_cycle(
        memloc.enforce, &engine_m, &enforce_r
    );

    EXPECT_GT(enforce_r.evicted_count, 0u);
}

/* ── ABA cycle tests ────────────────────────────────────────── */

TEST_F(QXMemlocConstitutionalTest, ABAConstitutionalCycleCompletes) {
    QXFlowAllocation alloc{};
    ASSERT_EQ(qx_flow_allocate(
        memloc.flow, "ui.glass.navbar.aba",
        "QLM-UI", QX_LEAF_CLASS_A,
        6u * 1048576u, &alloc
    ), QX_OK);

    QXEngineABAMeasurement  measurement{};
    QXEnforcementResult     enforcement{};

    QXResult r = qx_memloc_constitutional_cycle(
        &memloc,
        alloc.allocated_at_ms + 5000u,
        &measurement,
        &enforcement
    );
    EXPECT_EQ(r, QX_OK);
    EXPECT_GT(measurement.total_leaves_measured, 0u);

    QXFlowRelease release{};
    qx_flow_deallocate(memloc.flow, alloc.leaf, &release);
}

TEST_F(QXMemlocConstitutionalTest, ABAFormIsCompletePerCycle) {
    /* 1 — one identity */
    QXFlowAllocation alloc{};
    ASSERT_EQ(qx_flow_allocate(
        memloc.flow, "aba.form.complete.test",
        "QLM-CORE", QX_LEAF_CLASS_B,
        2u * 1048576u, &alloc
    ), QX_OK);
    /* The allocation itself is the 1 */
    EXPECT_NE(alloc.leaf, nullptr);
    EXPECT_NE(alloc.ptr,  nullptr);

    /* 2 — two states: committed and utilised */
    EXPECT_GT(alloc.soft_pressure_pct, 0.0);

    /* 4 — four laws enforced via cycle */
    QXEngineABAMeasurement  m{};
    QXEnforcementResult     e{};
    ASSERT_EQ(qx_memloc_constitutional_cycle(
        &memloc,
        alloc.allocated_at_ms + 5000u,
        &m, &e
    ), QX_OK);

    /* 4→2→1 — measurement contracts to one truth */
    EXPECT_GT(m.measured_x, 0.0);
    EXPECT_GE(m.compliant_leaves + m.warning_leaves +
              m.critical_leaves  + m.fatal_leaves,
              m.total_leaves_measured);

    /* 1 — return — Z.3 Pairs */
    QXFlowRelease release{};
    QXResult release_r = qx_flow_deallocate(
        memloc.flow, alloc.leaf, &release
    );
    if (release_r == QX_OK) {
        EXPECT_GT(release.released_bytes, 0u);
        EXPECT_GT(release.final_x, 0.0);
    } else {
        EXPECT_EQ(release_r, QX_ERR_DOUBLE_FREE);
        EXPECT_GT(e.evicted_count, 0u);
    }
}

/* ── Pressure tests ─────────────────────────────────────────── */

TEST_F(QXMemlocConstitutionalTest, PressureTier1EvietsClassD) {
    QXFlowAllocation alloc{};
    ASSERT_EQ(qx_flow_allocate(
        memloc.flow, "img.prefetch.pressure.d",
        "QLM-IMG", QX_LEAF_CLASS_D,
        8u * 1048576u, &alloc
    ), QX_OK);

    QXEnforcementResult result{};
    ASSERT_EQ(qx_enforce_pressure(
        memloc.enforce, 1u, &result
    ), QX_OK);

    EXPECT_GT(result.evicted_count,    0u);
    EXPECT_GT(result.bytes_released,   0u);
    EXPECT_GT(result.pages_decommitted,0u);
}

TEST_F(QXMemlocConstitutionalTest, PressureTier4PreservesClassA) {
    QXFlowAllocation class_a{};
    ASSERT_EQ(qx_flow_allocate(
        memloc.flow, "auth.state.protected",
        "QLM-CORE", QX_LEAF_CLASS_A,
        4u * 1048576u, &class_a
    ), QX_OK);

    QXEnforcementResult result{};
    ASSERT_EQ(qx_enforce_pressure(
        memloc.enforce, 4u, &result
    ), QX_OK);

    /* Class A must survive tier 4 */
    QXLeafXState state{};
    QXResult r = qx_signal_get_leaf_state(
        memloc.signal, "auth.state.protected", &state
    );
    EXPECT_EQ(r, QX_OK);
    EXPECT_FALSE(state.is_orphan_candidate);

    QXFlowRelease release{};
    qx_flow_deallocate(memloc.flow, class_a.leaf, &release);
}

/* ── Budget callback tests ──────────────────────────────────── */

static void test_budget_callback(
    const QXBudgetUpdate* update,
    void*                 user_data
) {
    bool* called = static_cast<bool*>(user_data);
    *called = true;
    EXPECT_NE(update, nullptr);
    EXPECT_GT(update->new_limit_bytes, 0u);
}

TEST_F(QXMemlocConstitutionalTest, BudgetCallbackFiredOnLifecycleChange) {
    bool called = false;

    ASSERT_EQ(qx_flow_register_budget_callback(
        memloc.flow,
        "QLM-IMG.imgDecoded",
        test_budget_callback,
        &called
    ), QX_OK);

    ASSERT_EQ(qx_flow_notify_lifecycle_change(
        memloc.flow, 1u /* background */
    ), QX_OK);

    EXPECT_TRUE(called);

    qx_flow_deregister_budget_callback(
        memloc.flow, "QLM-IMG.imgDecoded"
    );
}

TEST_F(QXMemlocConstitutionalTest, BudgetCallbackPairsRegisterDeregister) {
    bool called = false;

    ASSERT_EQ(qx_flow_register_budget_callback(
        memloc.flow,
        "QLM-NET.netAPI",
        test_budget_callback,
        &called
    ), QX_OK);

    /* Z.3 Pairs — deregister the callback */
    EXPECT_EQ(qx_flow_deregister_budget_callback(
        memloc.flow, "QLM-NET.netAPI"
    ), QX_OK);

    /* After deregistration callback must not fire */
    called = false;
    qx_flow_notify_lifecycle_change(memloc.flow, 0u);
    EXPECT_FALSE(called);
}

/* ── Pool tests ─────────────────────────────────────────────── */

TEST_F(QXMemlocConstitutionalTest, PoolUtilisationZeroAtStart) {
    double pct = qx_pool_total_utilisation_pct(&memloc.pool);
    EXPECT_EQ(pct, 0.0);
}

TEST_F(QXMemlocConstitutionalTest, PoolUtilisationIncreasesAfterAlloc) {
    QXFlowAllocation alloc{};
    ASSERT_EQ(qx_flow_allocate(
        memloc.flow, "pool.utilisation.check",
        "QLM-IMG", QX_LEAF_CLASS_C,
        32u * 1048576u, &alloc
    ), QX_OK);

    double pct = qx_pool_total_utilisation_pct(&memloc.pool);
    EXPECT_GT(pct, 0.0);

    QXFlowRelease release{};
    qx_flow_deallocate(memloc.flow, alloc.leaf, &release);
}

TEST_F(QXMemlocConstitutionalTest, PoolUtilisationReturnsToZeroAfterRelease) {
    QXFlowAllocation alloc{};
    ASSERT_EQ(qx_flow_allocate(
        memloc.flow, "pool.utilisation.return",
        "QLM-IMG", QX_LEAF_CLASS_C,
        32u * 1048576u, &alloc
    ), QX_OK);

    QXFlowRelease release{};
    qx_flow_deallocate(memloc.flow, alloc.leaf, &release);

    double pct = qx_pool_total_utilisation_pct(&memloc.pool);
    EXPECT_EQ(pct, 0.0);
}
