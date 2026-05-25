/* ============================================================
 * qx_memloc_enforce.h
 * QXMemloc — Ai [Api] — Enforcement Layer
 *
 * The fire that transforms. Constitutional enforcement
 * that cannot be ignored. When a law of Z is violated,
 * Api burns away what must be released to restore order.
 *
 * ALAMTOLOGI: Api transforms. It cannot be ignored.
 * The enforcement layer fires when X drifts fatally,
 * when pressure exceeds constitutional limits, or when
 * a leaf violates the Law of Z.
 *
 * All four Laws of Z enforced here:
 *   Z.1 [Pattern]     — identity must exist
 *   Z.2 [Limit]       — boundaries must hold
 *   Z.3 [Pairs]       — orphans must be resolved
 *   Z.4 [Equilibrium] — balance must be restored
 *
 * Owner : Masa Bayu
 * Date  : 2026-05-25
 * Repo  : https://github.com/qxengine/qxengine-core
 * ============================================================ */

#ifndef QX_MEMLOC_ENFORCE_H
#define QX_MEMLOC_ENFORCE_H

#include "qxengine/core/qx_types.h"
#include "qxengine/core/qx_error.h"
#include "qxengine/memory/qx_memloc_signal.h"
#include "qxengine/memory/qx_memloc_flow.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Enforcement result ─────────────────────────────────────── */

/*
 * QXEnforcementAction
 *
 * Records one enforcement action taken by the Api layer.
 * Every action is timestamped and logged for the audit trail.
 */
typedef enum {
    QX_ACTION_NONE            = 0,  /* no action required               */
    QX_ACTION_WARN_PATTERN    = 1,  /* Z.1 warning — label drift        */
    QX_ACTION_WARN_LIMIT      = 2,  /* Z.2 warning — approaching ceiling*/
    QX_ACTION_WARN_PAIRS      = 3,  /* Z.3 warning — orphan candidate   */
    QX_ACTION_WARN_EQUILIBRIUM= 4,  /* Z.4 warning — imbalance detected */
    QX_ACTION_EVICT_SOFT      = 5,  /* soft eviction — class D leaves   */
    QX_ACTION_EVICT_MODERATE  = 6,  /* moderate eviction — class C+D    */
    QX_ACTION_EVICT_CRITICAL  = 7,  /* critical eviction — class B+C+D  */
    QX_ACTION_EVICT_FATAL_X   = 8,  /* fatal X drift — evict any class  */
    QX_ACTION_DECOMMIT_PAGES  = 9,  /* physical pages returned to OS    */
    QX_ACTION_RESTORE_BUDGET  = 10  /* budget restored after recovery   */
} QXEnforcementAction;

/*
 * QXEnforcementResult
 *
 * The complete result of one enforcement cycle.
 * Records every action taken and every byte affected.
 */
typedef struct {
    QXEnforcementAction action;              /* primary action taken     */
    uint32_t            evicted_count;       /* leaves evicted           */
    QXSize              bytes_released;      /* bytes returned to pool   */
    QXSize              pages_decommitted;   /* physical pages released  */
    uint32_t            orphans_resolved;    /* orphan leaves resolved   */
    QXBool              equilibrium_restored;/* Z.4 restored this cycle  */
    QXTimestamp         enforced_at_ms;      /* timestamp                */
} QXEnforcementResult;

/* ── Enforce handle ─────────────────────────────────────────── */

/* Opaque enforcement layer handle */
typedef struct QXMemlocEnforce_s* QXMemlocEnforceHandle;

/* ── Lifecycle ──────────────────────────────────────────────── */

/*
 * qx_enforce_create
 *
 * Creates the enforcement layer over flow and signal layers.
 *
 * Thread safety: NOT thread-safe. Call once at startup.
 */
QX_API QXResult qx_enforce_create(
    QXMemlocFlowHandle      flow,
    QXMemlocSignalHandle    signal,
    QXMemlocEnforceHandle*  out_enforce
);

/*
 * qx_enforce_destroy
 *
 * Z.3 [Pairs] — created, must be destroyed.
 * Thread safety: NOT thread-safe.
 */
QX_API void qx_enforce_destroy(QXMemlocEnforceHandle enforce);

/* ── Constitutional enforcement ─────────────────────────────── */

/*
 * qx_enforce_cycle
 *
 * The primary enforcement function. Called every cognitive
 * cycle after qx_signal_measure_cycle.
 *
 * Enforces all four Laws of Z:
 *
 * Z.1 [Pattern]:
 *   — Verifies all active leaves have valid labels
 *   — Flags any leaf with empty or malformed identity
 *
 * Z.2 [Limit]:
 *   — Checks all segments against their soft/hard limits
 *   — Triggers eviction cascade if soft limit exceeded
 *   — Fires budget callbacks with reduced limits
 *
 * Z.3 [Pairs]:
 *   — Identifies orphan leaves (idle > 60 seconds)
 *   — Escalates orphan severity: WARNING → CRITICAL → FATAL
 *   — At FATAL: evicts orphan regardless of leaf class
 *
 * Z.4 [Equilibrium]:
 *   — Checks cross-segment balance
 *   — Flags overloaded segments (> 80% utilisation)
 *   — Flags starved segments (< 20% while others overflow)
 *   — Triggers rebalancing flow
 *
 * Universal Formula:
 *   — Processes fatal X drift leaves from signal layer
 *   — Evicts leaves where drift_ratio > QX_X_DRIFT_FATAL
 *   — Records constitutional violation in audit trail
 *
 * Thread safety: Thread-safe.
 */
QX_API QXResult qx_enforce_cycle(
    QXMemlocEnforceHandle         enforce,
    const QXEngineABAMeasurement* aba_measurement,
    QXEnforcementResult*          out_result
);

/*
 * qx_enforce_pressure
 *
 * Responds to OS memory pressure by tier.
 * Ai [Api] — fire transforms the allocation landscape.
 *
 * Tier 1 (soft):
 *   — Evict all class D (backgroundProportional) leaves
 *   — Decommit their physical pages immediately
 *   — Reduce all slot budgets by 30%
 *
 * Tier 2 (moderate):
 *   — Evict all class D leaves
 *   — Evict class C (priorityProportional) leaves
 *   — Decommit physical pages
 *   — Reduce all slot budgets by 50%
 *
 * Tier 3 (critical):
 *   — Evict class D, C, B leaves
 *   — Decommit physical pages
 *   — Reduce all slot budgets by 75%
 *
 * Tier 4 (imminentKill):
 *   — Evict all non-class-A leaves
 *   — Decommit maximum physical pages
 *   — Reduce all slot budgets to minimum viable
 *
 * Thread safety: Thread-safe.
 */
QX_API QXResult qx_enforce_pressure(
    QXMemlocEnforceHandle  enforce,
    uint8_t                pressure_tier,
    QXEnforcementResult*   out_result
);

/*
 * qx_enforce_restore
 *
 * Called when pressure tier decreases.
 * Restores budgets and recommits pages as RAM becomes
 * available again.
 * Ar [Air] — water flows back into the channels.
 *
 * Thread safety: Thread-safe.
 */
QX_API QXResult qx_enforce_restore(
    QXMemlocEnforceHandle  enforce,
    uint8_t                previous_tier,
    uint8_t                current_tier,
    QXEnforcementResult*   out_result
);

/*
 * qx_enforce_equilibrium
 *
 * Explicit equilibrium check and restoration.
 * Called by the cognitive cycle independent of pressure.
 * Z.4 [Equilibrium] — balance is continuously maintained.
 *
 * Thread safety: Thread-safe.
 */
QX_API QXResult qx_enforce_equilibrium(
    QXMemlocEnforceHandle  enforce,
    QXEnforcementResult*   out_result
);

#ifdef __cplusplus
}
#endif

#endif /* QX_MEMLOC_ENFORCE_H */
