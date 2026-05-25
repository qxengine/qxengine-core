/* ============================================================
 * qx_memloc_signal.h
 * QXMemloc — An [Angin] — Signal Layer
 *
 * The wind that carries measurement. Invisible but real.
 * Every 5 seconds the cognitive cycle measures X = m/t
 * for every active leaf and reports constitutional truth.
 *
 * ALAMTOLOGI: Angin moves invisibly. Its effects are
 * measurable. The signal layer carries the Universal
 * Formula measurement through the entire system.
 *
 * Universal Formula: X = m / t
 *   m = time held (milliseconds)
 *   t = bytes normalised against soft budget
 *   X = constitutional flow rate constant
 *
 * Owner : Masa Bayu
 * Date  : 2026-05-25
 * Repo  : https://github.com/qxengine/qxengine-core
 * ============================================================ */

#ifndef QX_MEMLOC_SIGNAL_H
#define QX_MEMLOC_SIGNAL_H

#include "qxengine/core/qx_types.h"
#include "qxengine/core/qx_error.h"
#include "qxengine/memory/qx_memloc_pool.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── X measurement tolerance ────────────────────────────────── */

/* Maximum drift ratio before WARNING                           */
#define QX_X_DRIFT_WARNING    0.25

/* Maximum drift ratio before CRITICAL                         */
#define QX_X_DRIFT_CRITICAL   0.50

/* Maximum drift ratio before FATAL — evict regardless of class */
#define QX_X_DRIFT_FATAL      1.00

/* ── Leaf X state ───────────────────────────────────────────── */

/*
 * QXLeafXState
 *
 * Tracks the Universal Formula state for one leaf.
 * Attached to every active leaf at allocation time.
 * Updated every cognitive cycle.
 *
 * X = m / t
 */
typedef struct {
    char        leaf_id[128];         /* Z.1 [Pattern] — identity       */
    QXTimestamp allocated_at_ms;      /* m starts here                  */
    QXTimestamp last_touched_ms;      /* orphan detection reference      */
    QXSize      reserved_bytes;       /* t — energy of this leaf         */
    double      declared_x;           /* X declared in manifest          */
    double      measured_x;           /* X = m/t measured this cycle     */
    double      drift_ratio;          /* |measured - declared| / declared*/
    double      cumulative_x;         /* running average X               */
    uint32_t    measurement_count;    /* number of cycles measured       */
    QXBool      is_orphan_candidate;  /* Z.3 — not touched for 60s       */
} QXLeafXState;

/* ── ABA measurements ───────────────────────────────────────── */

/*
 * QXLeafABAMeasurement
 *
 * The ABA measurement for one leaf.
 * Produced every cognitive cycle for every active leaf.
 *
 * 1→2→4|4→2→1 expressed as a measurable engineering value.
 */
typedef struct {
    char        leaf_id[128];
    double      declared_x;
    double      measured_x;
    double      drift_ratio;
    QXBool      is_constant;
    QXBool      is_orphan;
    QXTimestamp measured_at_ms;
} QXLeafABAMeasurement;

/*
 * QXSegmentABAMeasurement
 *
 * The ABA measurement for one segment.
 * Aggregated from all active leaf measurements.
 * Used by Z.4 [Equilibrium] enforcement.
 */
typedef struct {
    char        segment_id[16];
    double      declared_x;
    double      measured_x;
    double      drift_ratio;
    QXBool      is_constant;
    uint32_t    violating_leaf_count;
    uint32_t    compliant_leaf_count;
    uint32_t    orphan_leaf_count;
    QXTimestamp measured_at_ms;
} QXSegmentABAMeasurement;

/*
 * QXEngineABAMeasurement
 *
 * The engine-level ABA measurement.
 * The single constitutional truth of the entire system
 * for this cognitive cycle.
 *
 * This is X at the engine level — the Universal Formula
 * applied to the complete allocation system.
 */
typedef struct {
    double      declared_x;
    double      measured_x;
    double      drift_ratio;
    QXBool      is_constant;
    uint32_t    total_leaves_measured;
    uint32_t    compliant_leaves;
    uint32_t    warning_leaves;
    uint32_t    critical_leaves;
    uint32_t    fatal_leaves;
    uint32_t    orphan_leaves;
    QXTimestamp measured_at_ms;
} QXEngineABAMeasurement;

/* ── Signal handle ──────────────────────────────────────────── */

/* Opaque signal layer handle */
typedef struct QXMemlocSignal_s* QXMemlocSignalHandle;

/* ── Lifecycle ──────────────────────────────────────────────── */

/*
 * qx_signal_create
 *
 * Creates the signal layer over an existing pool.
 * declared_x is the constitutional X from the manifest.
 *
 * Thread safety: NOT thread-safe. Call once at startup.
 */
QX_API QXResult qx_signal_create(
    const QXMemlocPool*    pool,
    double                 declared_x,
    double                 tolerance_pct,
    QXMemlocSignalHandle*  out_signal
);

/*
 * qx_signal_destroy
 *
 * Z.3 [Pairs] — created, must be destroyed.
 * Thread safety: NOT thread-safe.
 */
QX_API void qx_signal_destroy(QXMemlocSignalHandle signal);

/* ── Leaf tracking ──────────────────────────────────────────── */

/*
 * qx_signal_register_leaf
 *
 * Registers a new leaf with the signal layer at allocation.
 * Initialises QXLeafXState with declared_x and birth time.
 * Called automatically by qx_flow_allocate.
 *
 * Thread safety: Thread-safe.
 */
QX_API QXResult qx_signal_register_leaf(
    QXMemlocSignalHandle signal,
    const char*          leaf_id,
    QXSize               reserved_bytes,
    QXTimestamp          allocated_at_ms
);

/*
 * qx_signal_deregister_leaf
 *
 * Removes a leaf from signal tracking at deallocation.
 * Records final X measurement before removal.
 * Called automatically by qx_flow_deallocate.
 *
 * Thread safety: Thread-safe.
 */
QX_API QXResult qx_signal_deregister_leaf(
    QXMemlocSignalHandle    signal,
    const char*             leaf_id,
    QXLeafABAMeasurement*   out_final_measurement
);

/*
 * qx_signal_touch_leaf
 *
 * Records activity on a leaf — resets orphan timer.
 * Called automatically by qx_flow_touch.
 *
 * Thread safety: Thread-safe.
 */
QX_API QXResult qx_signal_touch_leaf(
    QXMemlocSignalHandle signal,
    const char*          leaf_id,
    QXTimestamp          touched_at_ms
);

/* ── Cognitive cycle ────────────────────────────────────────── */

/*
 * qx_signal_measure_cycle
 *
 * The core cognitive cycle measurement function.
 * Called every 5 seconds by the engine evaluation loop.
 *
 * For every active leaf:
 *   1. Computes m = now_ms - allocated_at_ms
 *   2. Computes t = reserved_bytes / soft_budget_bytes
 *   3. Computes X = m / t
 *   4. Computes drift = |X - declared_x| / declared_x
 *   5. Classifies: compliant / warning / critical / fatal
 *   6. Checks orphan: last_touched > QX_ORPHAN_IDLE_SECONDS
 *
 * Produces QXEngineABAMeasurement and per-segment measurements.
 *
 * An [Angin] — the wind measures every channel every cycle.
 *
 * Thread safety: Thread-safe.
 */
QX_API QXResult qx_signal_measure_cycle(
    QXMemlocSignalHandle      signal,
    QXTimestamp               now_ms,
    QXEngineABAMeasurement*   out_engine_measurement,
    QXSegmentABAMeasurement*  out_segment_measurements,
    uint32_t                  segment_count
);

/*
 * qx_signal_get_leaf_state
 *
 * Returns the current X state of a specific leaf.
 * Used by the enforcement layer for targeted eviction.
 *
 * Thread safety: Thread-safe.
 */
QX_API QXResult qx_signal_get_leaf_state(
    QXMemlocSignalHandle signal,
    const char*          leaf_id,
    QXLeafXState*        out_state
);

#ifdef __cplusplus
}
#endif

#endif /* QX_MEMLOC_SIGNAL_H */
