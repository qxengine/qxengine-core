/* ============================================================
 * qx_memloc_signal_internal.h
 * QXMemloc — An [Angin] — Signal Layer Internal Definitions
 *
 * Shared internal types for the three signal implementation
 * files. Not part of the public API.
 *
 * ALAMTOLOGI — Quranic Science
 * Founded by Masa Bayu
 *
 * Owner : Masa Bayu
 * Date  : 2026-05-25
 * Repo  : https://github.com/qxengine/qxengine-core
 * ============================================================ */

#ifndef QX_MEMLOC_SIGNAL_INTERNAL_H
#define QX_MEMLOC_SIGNAL_INTERNAL_H

#include "qxengine/intelligence/qx_snapshot_types.h"
#include "qxengine/memory/qx_memloc_signal.h"
#include "qx_memloc_flow_internal.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#if defined(__APPLE__) || defined(__linux__) || \
    defined(__ANDROID__)
#   include <pthread.h>
#endif

/* ── Internal constants ─────────────────────────────────────── */

/*
 * Maximum leaves tracked simultaneously.
 * Matches QX_FLOW_MAX_LEAVES in the flow layer.
 * An [Angin] measures exactly what Ar [Air] holds.
 */
#define QX_SIGNAL_MAX_LEAVES        256u

/*
 * Orphan idle threshold in milliseconds.
 * A leaf not touched for this duration is an orphan
 * candidate — a Z.3 [Pairs] violation.
 * 60 seconds as defined in qx_constants.h.
 */
#define QX_SIGNAL_ORPHAN_MS         (60u * 1000u)

/*
 * Minimum normalised t value to avoid division by zero
 * in the X = m/t formula.
 * Represents 1 byte against the soft budget — effectively
 * the smallest meaningful t value.
 */
#define QX_SIGNAL_MIN_T             1.0e-12

/* ── Internal leaf X state registry ────────────────────────── */

/*
 * QXSignalLeafEntry
 *
 * Internal entry tracking one leaf's X state.
 * The wind knows every leaf it must measure.
 *
 * An [Angin] — one entry per active leaf.
 * Born when the leaf is registered.
 * Released when the leaf is deregistered.
 */
typedef struct {
    char        leaf_id[128];         /* Z.1 — identity                 */
    char        segment_id[16];       /* which segment this leaf is in  */
    QXTimestamp allocated_at_ms;      /* m starts here                  */
    QXTimestamp last_touched_ms;      /* orphan detection reference     */
    QXSize      reserved_bytes;       /* t — energy of this leaf        */
    double      declared_x;           /* X declared at engine startup   */
    double      tolerance;            /* acceptable drift fraction      */
    double      measured_x;           /* X = m/t last measurement       */
    double      drift_ratio;          /* |measured-declared|/declared   */
    double      cumulative_x_sum;     /* running sum for average        */
    uint32_t    measurement_count;    /* number of cycles measured      */
    QXBool      is_active;            /* true until deregistered        */
    QXBool      is_orphan_candidate;  /* true if idle > orphan_ms       */
} QXSignalLeafEntry;

/*
 * QXMemlocSignal_s
 *
 * The internal signal layer state.
 * The wind itself — invisible but fully aware of every
 * leaf in every channel at every moment.
 */
struct QXMemlocSignal_s {
    /* ── Pool reference ─────────────────────────────────── */
    const QXMemlocPool*   pool;
    QXMemlocFlowHandle    flow;

    /* ── Leaf registry ──────────────────────────────────── */
    QXSignalLeafEntry     leaves[QX_SIGNAL_MAX_LEAVES];
    uint32_t              leaf_count;
    pthread_mutex_t       leaf_mutex;

    /* ── Constitutional X constants ─────────────────────── */
    double                declared_x;
    double                tolerance_pct;

    /* ── Cycle statistics ───────────────────────────────── */
    uint64_t              total_cycles;
    QXTimestamp           last_cycle_ms;

    /* ── Initialisation flag ─────────────────────────────── */
    QXBool                is_initialised;
};

/* ── Internal cycle helpers ────────────────────────────────── */

QXResult qx_signal_build_snapshot_input(
    QXMemlocSignalHandle          signal,
    const QXEngineABAMeasurement* aba,
    uint8_t                       pressure_tier,
    double                        law_health_score,
    uint32_t                      compliance_streak,
    double                        native_utilisation,
    QXTimestamp                   now_ms,
    QXSnapshotInput*              out_input
);

QXResult qx_signal_cycle_stats(
    QXMemlocSignalHandle signal,
    uint64_t*            out_total_cycles,
    QXTimestamp*         out_last_cycle_ms,
    uint32_t*            out_active_leaf_count
);

void qx_signal_attach_flow(
    QXMemlocSignalHandle signal,
    QXMemlocFlowHandle   flow
);

void qx_signal_sync_from_flow(QXMemlocSignalHandle signal);

/* ── Shared inline helpers ──────────────────────────────────── */

/*
 * signal_now_ms
 *
 * Returns current time in milliseconds since Unix epoch.
 */
static inline QXTimestamp signal_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (QXTimestamp)(
        (uint64_t)ts.tv_sec  * 1000u +
        (uint64_t)ts.tv_nsec / 1000000u
    );
}

/*
 * signal_find_leaf
 *
 * Finds an active leaf entry by leaf_id.
 * Returns NULL if not found or inactive.
 * Must be called with leaf_mutex LOCKED.
 */
static inline QXSignalLeafEntry* signal_find_leaf(
    struct QXMemlocSignal_s* signal,
    const char*              leaf_id)
{
    uint32_t i;
    if (leaf_id == NULL) {
        return NULL;
    }
    for (i = 0u; i < QX_SIGNAL_MAX_LEAVES; ++i) {
        if (signal->leaves[i].is_active &&
            strncmp(signal->leaves[i].leaf_id,
                    leaf_id, 128u) == 0) {
            return &signal->leaves[i];
        }
    }
    return NULL;
}

/*
 * signal_find_free_slot
 *
 * Returns the index of the first inactive slot.
 * Returns QX_SIGNAL_MAX_LEAVES if registry is full.
 */
static inline uint32_t signal_find_free_slot(
    const struct QXMemlocSignal_s* signal)
{
    uint32_t i;
    for (i = 0u; i < QX_SIGNAL_MAX_LEAVES; ++i) {
        if (!signal->leaves[i].is_active) {
            return i;
        }
    }
    return QX_SIGNAL_MAX_LEAVES;
}

/*
 * signal_compute_x
 *
 * Computes X = m/t for one leaf at a given timestamp.
 *
 * Universal Formula:
 *   m = now_ms - allocated_at_ms  (time held)
 *   t = reserved_bytes / soft_budget_bytes (normalised)
 *   X = m / t
 *
 * t is normalised so X is dimensionless and comparable
 * across leaves of any size on any device.
 *
 * An [Angin] — the wind measures the flow rate
 * of every drop of water in every channel.
 */
static inline double signal_compute_x(
    const QXSignalLeafEntry* entry,
    QXTimestamp              now,
    QXSize                   soft_budget_bytes)
{
    double m;
    double t;

    m = (double)(now - entry->allocated_at_ms);

    t = (soft_budget_bytes > 0u)
        ? ((double)entry->reserved_bytes /
           (double)soft_budget_bytes)
        : 1.0;

    if (t < QX_SIGNAL_MIN_T) {
        t = QX_SIGNAL_MIN_T;
    }

    return m / t;
}

/*
 * signal_compute_drift
 *
 * Computes the drift ratio between measured and declared X.
 *
 *   drift = |measured_x - declared_x| / declared_x
 *
 * A drift of 0.0 means X is perfectly constant.
 * A drift of 1.0 means X has doubled or halved — FATAL.
 */
static inline double signal_compute_drift(
    double measured_x,
    double declared_x)
{
    double diff;

    if (declared_x <= 0.0) {
        return 0.0;
    }

    diff = measured_x - declared_x;
    if (diff < 0.0) {
        diff = -diff;
    }

    return diff / declared_x;
}

#endif /* QX_MEMLOC_SIGNAL_INTERNAL_H */
