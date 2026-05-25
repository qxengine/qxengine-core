/* ============================================================
 * qx_memloc_flow.h
 * QXMemloc — Ar [Air] — Flow Layer
 *
 * The water in the channels. Dynamic. Constitutional.
 * Governs how memory flows from the pool to subsystems
 * and returns when the need is gone.
 *
 * ALAMTOLOGI: Air is water. Water flows, finds its level,
 * fills its container exactly, and always returns to its
 * source. Every byte in QXMemloc is a drop of water.
 *
 * Law of Z.1 [Pattern]     — every flow has identity
 * Law of Z.2 [Limit]       — every flow has a ceiling
 * Law of Z.3 [Pairs]       — every flow has a return
 * Law of Z.4 [Equilibrium] — flows balance across segments
 *
 * Owner : Masa Bayu
 * Date  : 2026-05-25
 * Repo  : https://github.com/qxengine/qxengine-core
 * ============================================================ */

#ifndef QX_MEMLOC_FLOW_H
#define QX_MEMLOC_FLOW_H

#include "qxengine/core/qx_types.h"
#include "qxengine/core/qx_error.h"
#include "qxengine/memory/qx_memloc_pool.h"
#include "qxengine/memory/qx_leaf.h"
#include "qxengine/memory/qx_segment.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Flow result ────────────────────────────────────────────── */

/*
 * QXFlowAllocation
 *
 * The complete result of one constitutional allocation.
 * The pointer and the leaf handle are inseparable —
 * born together, they must die together.
 *
 * Z.3 [Pairs] — ptr and leaf are one constitutional pair.
 * The caller receives both or neither.
 */
typedef struct {
    void*        ptr;                  /* real pointer into pool         */
    QXLeafHandle leaf;                 /* constitutional leaf handle     */
    QXSize       usable_bytes;         /* actual usable size             */
    double       soft_pressure_pct;    /* pool utilisation after alloc   */
    uint8_t      pressure_tier;        /* current tier after alloc       */
    uint32_t     evicted_count;        /* leaves evicted to make room    */
    QXTimestamp  allocated_at_ms;      /* Z.1 — birth timestamp          */
} QXFlowAllocation;

/*
 * QXFlowRelease
 *
 * The complete result of one constitutional deallocation.
 * Records the constitutional return of water to the river.
 *
 * Z.3 [Pairs] — every QXFlowAllocation has a QXFlowRelease.
 */
typedef struct {
    QXSize      released_bytes;        /* bytes returned to pool         */
    double      soft_pressure_pct;     /* pool utilisation after release */
    uint8_t     pressure_tier;         /* current tier after release     */
    QXTimestamp released_at_ms;        /* Z.3 — return timestamp         */
    double      final_x;               /* X = m/t at time of release     */
} QXFlowRelease;

/*
 * QXBudgetUpdate
 *
 * Pushed to registered subsystems when QXMemloc recalculates
 * budgets due to lifecycle change, screen profile change,
 * or pressure tier change.
 *
 * Ar [Air] — the channel width changes, subsystems adjust.
 */
typedef struct {
    char        segment_id[16];        /* which segment                  */
    char        slot_id[64];           /* which slot                     */
    QXSize      new_limit_bytes;       /* new constitutional ceiling     */
    QXSize      previous_limit_bytes;  /* previous ceiling               */
    uint8_t     pressure_tier;         /* tier driving this update       */
    uint8_t     lifecycle_state;       /* foreground or background       */
    QXTimestamp updated_at_ms;         /* timestamp of recalculation     */
} QXBudgetUpdate;

/* ── Budget callback ────────────────────────────────────────── */

/*
 * QXBudgetCallback
 *
 * Registered by subsystems to receive budget updates.
 * Called on the cognitive cycle thread — must be non-blocking.
 *
 * An [Angin] — the signal carried on the wind to each field.
 */
typedef void (*QXBudgetCallback)(
    const QXBudgetUpdate* update,
    void*                 user_data
);

/*
 * QXUsageReport
 *
 * Submitted by subsystems every cognitive cycle to report
 * actual current usage back to QXMemloc.
 *
 * An [Angin] — the measurement of actual flow in the channel.
 */
typedef struct {
    char        slot_id[64];           /* which slot is reporting        */
    QXSize      current_bytes;         /* bytes currently held           */
    QXSize      peak_bytes;            /* peak since last report         */
    QXTimestamp reported_at_ms;        /* timestamp of this report       */
} QXUsageReport;

/* ── Flow handle ────────────────────────────────────────────── */

/* Opaque flow controller handle */
typedef struct QXMemlocFlow_s* QXMemlocFlowHandle;

/* ── Lifecycle ──────────────────────────────────────────────── */

/*
 * qx_flow_create
 *
 * Creates the flow controller over an existing pool.
 * The pool must already be initialised (qx_pool_create).
 *
 * Thread safety: NOT thread-safe. Call once at engine startup.
 */
QX_API QXResult qx_flow_create(
    QXMemlocPool*       pool,
    QXMemlocFlowHandle* out_flow
);

/*
 * qx_flow_destroy
 *
 * Destroys the flow controller and releases all internal state.
 * Z.3 [Pairs] — created, must be destroyed.
 *
 * Thread safety: NOT thread-safe. Call once at engine shutdown.
 */
QX_API void qx_flow_destroy(QXMemlocFlowHandle flow);

/* ── Allocation and release ─────────────────────────────────── */

/*
 * qx_flow_allocate
 *
 * The constitutional allocation function.
 * Enforces all four Laws of Z before committing bytes.
 *
 * Flow:
 *   1 [Pattern]     — label validated, identity confirmed
 *   2 [Limit]       — budget checked, ceiling respected
 *   4 [Pairs]       — return path registered
 *   4→2→1           — X declared, leaf born, pointer returned
 *
 * Returns QXFlowAllocation with real ptr and leaf handle.
 * ptr is NULL and leaf is NULL on any error.
 *
 * Thread safety: Thread-safe.
 */
QX_API QXResult qx_flow_allocate(
    QXMemlocFlowHandle  flow,
    const char*         label,
    const char*         segment_id,
    QXLeafClassId       leaf_class,
    QXSize              size_bytes,
    QXFlowAllocation*   out_allocation
);

/*
 * qx_flow_deallocate
 *
 * The constitutional deallocation function.
 * Returns water to the river. Retires the leaf.
 * Records the final X measurement.
 *
 * Z.3 [Pairs] — the pair of qx_flow_allocate.
 * Must be called for every successful allocation.
 *
 * Thread safety: Thread-safe.
 */
QX_API QXResult qx_flow_deallocate(
    QXMemlocFlowHandle  flow,
    QXLeafHandle        leaf,
    QXFlowRelease*      out_release
);

/*
 * qx_flow_touch
 *
 * Records activity on a leaf — resets the orphan timer.
 * A leaf that is touched is alive and in use.
 * A leaf not touched for QX_ORPHAN_IDLE_SECONDS is
 * flagged as a Z.3 [Pairs] violation candidate.
 *
 * Thread safety: Thread-safe.
 */
QX_API QXResult qx_flow_touch(
    QXMemlocFlowHandle flow,
    QXLeafHandle       leaf
);

/* ── Budget push ────────────────────────────────────────────── */

/*
 * qx_flow_register_budget_callback
 *
 * Registers a subsystem to receive budget updates.
 * Called when budgets change due to lifecycle, screen
 * profile, or pressure tier changes.
 *
 * Ar [Air] — the channel notifies the field of new capacity.
 *
 * Thread safety: Thread-safe.
 */
QX_API QXResult qx_flow_register_budget_callback(
    QXMemlocFlowHandle flow,
    const char*        slot_id,
    QXBudgetCallback   callback,
    void*              user_data
);

/*
 * qx_flow_deregister_budget_callback
 *
 * Removes a previously registered budget callback.
 * Z.3 [Pairs] — registered, must be deregistered.
 *
 * Thread safety: Thread-safe.
 */
QX_API QXResult qx_flow_deregister_budget_callback(
    QXMemlocFlowHandle flow,
    const char*        slot_id
);

/*
 * qx_flow_report_usage
 *
 * Subsystems report actual current usage back to QXMemloc.
 * Feeds the An [Angin] signal layer with real measurements.
 *
 * Thread safety: Thread-safe.
 */
QX_API QXResult qx_flow_report_usage(
    QXMemlocFlowHandle  flow,
    const QXUsageReport* report
);

/* ── Recalculation triggers ─────────────────────────────────── */

/*
 * qx_flow_notify_lifecycle_change
 *
 * Triggers budget recalculation for all slots when the
 * app lifecycle changes (foreground ↔ background).
 * Fires all registered budget callbacks with new limits.
 *
 * Thread safety: Thread-safe.
 */
QX_API QXResult qx_flow_notify_lifecycle_change(
    QXMemlocFlowHandle flow,
    uint8_t            lifecycle_state
);

/*
 * qx_flow_notify_screen_profile_change
 *
 * Triggers budget recalculation for all slots when the
 * active screen profile changes.
 * Fires all registered budget callbacks with new limits.
 *
 * Thread safety: Thread-safe.
 */
QX_API QXResult qx_flow_notify_screen_profile_change(
    QXMemlocFlowHandle flow,
    const char*        screen_profile_id
);

/*
 * qx_flow_notify_pressure_change
 *
 * Triggers budget reduction for all slots when pressure
 * tier escalates. Decommits physical pages for evicted
 * leaves. Fires all registered budget callbacks.
 *
 * Ai [Api] — pressure transforms the allocation landscape.
 *
 * Thread safety: Thread-safe.
 */
QX_API QXResult qx_flow_notify_pressure_change(
    QXMemlocFlowHandle flow,
    uint8_t            pressure_tier
);

#ifdef __cplusplus
}
#endif

#endif /* QX_MEMLOC_FLOW_H */
