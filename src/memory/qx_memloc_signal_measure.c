/* ============================================================
 * qx_memloc_signal_measure.c
 * QXMemloc — An [Angin] — X = m/t Measurement
 *
 * The core measurement engine of the signal layer.
 * For every active leaf, the wind computes:
 *
 *   X = m / t
 *
 * Where:
 *   m = time held in milliseconds
 *   t = bytes normalised against soft budget
 *   X = constitutional flow rate constant
 *
 * ALAMTOLOGI — Quranic Science
 * Founded by Masa Bayu
 *
 * Universal Formula enforcement:
 *   drift < WARNING   — leaf is constitutionally compliant
 *   drift < CRITICAL  — leaf is drifting, warning issued
 *   drift < FATAL     — leaf is critically drifted
 *   drift >= FATAL    — leaf has broken X, must be evicted
 *
 * Owner : Masa Bayu
 * Date  : 2026-05-25
 * Repo  : https://github.com/qxengine/qxengine-core
 * ============================================================ */

#include "qx_memloc_signal_internal.h"

/* ── Internal: classify X drift ────────────────────────────── */

/*
 * classify_drift
 *
 * Maps a drift ratio to a measurement classification.
 * Used to populate the QXEngineABAMeasurement counters.
 *
 * An [Angin] — the wind classifies what it measures.
 * Classification drives enforcement in Ai [Api].
 */
typedef enum {
    QX_DRIFT_COMPLIANT = 0,
    QX_DRIFT_WARNING   = 1,
    QX_DRIFT_CRITICAL  = 2,
    QX_DRIFT_FATAL     = 3
} QXDriftClass;

static QXDriftClass classify_drift(
    double drift_ratio,
    double tolerance)
{
    /*
     * Tolerance defines the compliant zone.
     * Beyond tolerance the drift escalates through
     * warning, critical, and fatal thresholds.
     *
     * Z.2 [Limit] — X has constitutional boundaries.
     * Exceeding them is a Limit violation.
     */
    if (drift_ratio <= tolerance) {
        return QX_DRIFT_COMPLIANT;
    }
    if (drift_ratio <= QX_X_DRIFT_WARNING) {
        return QX_DRIFT_WARNING;
    }
    if (drift_ratio <= QX_X_DRIFT_CRITICAL) {
        return QX_DRIFT_CRITICAL;
    }
    return QX_DRIFT_FATAL;
}

/* ── Internal: measure one leaf ─────────────────────────────── */

/*
 * measure_one_leaf
 *
 * Computes X = m/t for a single leaf at now_ms.
 * Updates the leaf entry with the new measurement.
 * Checks orphan status against the idle threshold.
 *
 * Must be called with leaf_mutex LOCKED.
 */
static QXDriftClass measure_one_leaf(
    QXSignalLeafEntry* entry,
    QXTimestamp        now,
    QXSize             soft_budget_bytes)
{
    double       measured_x;
    double       drift;
    QXDriftClass drift_class;

    /* Compute X = m/t */
    measured_x = signal_compute_x(
        entry, now, soft_budget_bytes
    );

    /* Compute drift ratio */
    drift = signal_compute_drift(
        measured_x, entry->declared_x
    );

    /* Update entry */
    entry->measured_x    = measured_x;
    entry->drift_ratio   = drift;
    entry->cumulative_x_sum += measured_x;
    entry->measurement_count++;

    /*
     * Z.3 [Pairs] — orphan detection.
     * Water that has stopped flowing is stagnant.
     * Stagnant water violates Pairs — it was taken
     * but is no longer being used or returned.
     */
    if ((now - entry->last_touched_ms) >=
        QX_SIGNAL_ORPHAN_MS) {
        entry->is_orphan_candidate = QX_TRUE;
    }

    /* Classify drift */
    drift_class = classify_drift(
        drift, entry->tolerance
    );

    return drift_class;
}

/* ── Internal: measure one segment ─────────────────────────── */

/*
 * measure_one_segment
 *
 * Aggregates leaf measurements for one segment.
 * Produces a QXSegmentABAMeasurement.
 *
 * Must be called with leaf_mutex LOCKED.
 */
static void measure_one_segment(
    struct QXMemlocSignal_s* signal,
    const char*              segment_id,
    QXTimestamp              now,
    QXSegmentABAMeasurement* out_seg)
{
    uint32_t i;
    double   x_sum         = 0.0;
    uint32_t counted       = 0u;

    memset(out_seg, 0, sizeof(QXSegmentABAMeasurement));

    strncpy(out_seg->segment_id, segment_id,
            sizeof(out_seg->segment_id) - 1u);

    out_seg->declared_x    = signal->declared_x;
    out_seg->measured_at_ms= now;

    for (i = 0u; i < QX_SIGNAL_MAX_LEAVES; ++i) {
        QXSignalLeafEntry* entry = &signal->leaves[i];

        if (!entry->is_active) {
            continue;
        }
        if (strncmp(entry->segment_id,
                    segment_id, 16u) != 0) {
            continue;
        }

        x_sum += entry->measured_x;
        counted++;

        if (entry->is_orphan_candidate) {
            out_seg->orphan_leaf_count++;
        }

        if (entry->drift_ratio <=
            signal->tolerance_pct / 100.0) {
            out_seg->compliant_leaf_count++;
        } else {
            out_seg->violating_leaf_count++;
        }
    }

    if (counted > 0u) {
        out_seg->measured_x = x_sum / (double)counted;
    } else {
        out_seg->measured_x = signal->declared_x;
    }

    out_seg->drift_ratio = signal_compute_drift(
        out_seg->measured_x, signal->declared_x
    );

    out_seg->is_constant = (QXBool)(
        out_seg->drift_ratio <=
        signal->tolerance_pct / 100.0
    );
}

/* ── qx_signal_measure_cycle ────────────────────────────────── */

QXResult qx_signal_measure_cycle(
    QXMemlocSignalHandle      signal,
    QXTimestamp               now_ms,
    QXEngineABAMeasurement*   out_engine,
    QXSegmentABAMeasurement*  out_segments,
    uint32_t                  segment_count)
{
    uint32_t i;
    double   x_sum          = 0.0;
    uint32_t measured_count = 0u;

    static const char* seg_ids[QX_POOL_SEGMENT_COUNT] = {
        "QLM-CORE", "QLM-UI",  "QLM-DATA",
        "QLM-IMG",  "QLM-NET", "QLM-AI",
        "QLM-SEC",  "QLM-LOG", "QLM-TEMP"
    };

    if (signal == NULL || !signal->is_initialised) {
        return QX_ERR_NULL_HANDLE;
    }
    if (out_engine == NULL) {
        return QX_ERR_NULL_HANDLE;
    }
    if (out_segments != NULL &&
        segment_count < QX_POOL_SEGMENT_COUNT) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    memset(out_engine, 0, sizeof(QXEngineABAMeasurement));

    out_engine->declared_x    = signal->declared_x;
    out_engine->measured_at_ms= now_ms;

    qx_signal_sync_from_flow(signal);

    pthread_mutex_lock(&signal->leaf_mutex);

    /* ── Measure every active leaf ──────────────────────── */

    for (i = 0u; i < QX_SIGNAL_MAX_LEAVES; ++i) {
        QXSignalLeafEntry* entry = &signal->leaves[i];
        QXDriftClass       dc;

        if (!entry->is_active) {
            continue;
        }

        /*
         * An [Angin] — the wind measures each leaf.
         * X = m/t. Drift classified. Orphan checked.
         * The wind does not judge — it only measures.
         * Ai [Api] will judge and enforce.
         */
        dc = measure_one_leaf(
            entry, now_ms,
            signal->pool->total_soft_limit_bytes
        );

        x_sum += entry->measured_x;
        measured_count++;

        out_engine->total_leaves_measured++;

        if (dc == QX_DRIFT_COMPLIANT) {
            out_engine->compliant_leaves++;
        } else if (dc == QX_DRIFT_WARNING) {
            out_engine->warning_leaves++;
        } else if (dc == QX_DRIFT_CRITICAL) {
            out_engine->critical_leaves++;
        } else {
            out_engine->fatal_leaves++;
        }

        if (entry->is_orphan_candidate) {
            out_engine->orphan_leaves++;
        }
    }

    /* ── Compute engine-level X ─────────────────────────── */

    /*
     * X at the engine level is the average X across
     * all active leaves. When all leaves are at their
     * declared X, the engine X equals the declared X.
     * This is constitutional equilibrium — the whole
     * system breathing at the right rate.
     *
     * Z.4 [Equilibrium] — the system is balanced
     * when its measured X equals its declared X.
     */
    if (measured_count > 0u) {
        out_engine->measured_x =
            x_sum / (double)measured_count;
    } else {
        out_engine->measured_x = signal->declared_x;
    }

    out_engine->drift_ratio = signal_compute_drift(
        out_engine->measured_x, signal->declared_x
    );

    out_engine->is_constant = (QXBool)(
        out_engine->drift_ratio <=
        signal->tolerance_pct / 100.0
    );

    /* ── Measure per-segment aggregates ─────────────────── */

    if (out_segments != NULL) {
        uint32_t s;
        for (s = 0u; s < QX_POOL_SEGMENT_COUNT &&
                     s < segment_count; ++s) {
            measure_one_segment(
                signal,
                seg_ids[s],
                now_ms,
                &out_segments[s]
            );
        }
    }

    signal->total_cycles++;
    signal->last_cycle_ms = now_ms;

    pthread_mutex_unlock(&signal->leaf_mutex);
    return QX_OK;
}
