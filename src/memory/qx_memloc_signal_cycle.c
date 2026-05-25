/* ============================================================
 * qx_memloc_signal_cycle.c
 * QXMemloc — An [Angin] — Cognitive Cycle Integration
 *
 * Bridges the signal layer to the constitutional cycle
 * defined in qx_memloc_constitutional.h.
 *
 * The cognitive cycle is the heartbeat of QXEngine.
 * Every 5 seconds the wind sweeps through the system.
 * Every leaf is measured. Every X is computed.
 * Every orphan is identified. Every drift is classified.
 *
 * ALAMTOLOGI — Quranic Science
 * Founded by Masa Bayu
 *
 * ABA in the cognitive cycle:
 *   1 → one cycle identity (timestamp, sequence)
 *   2 → two measurement phases (leaves, segments)
 *   4 → four drift classifications per leaf
 *   4→2→1 → segment aggregates → engine X → one truth
 *
 * Owner : Masa Bayu
 * Date  : 2026-05-25
 * Repo  : https://github.com/qxengine/qxengine-core
 * ============================================================ */

#include "qx_memloc_signal_internal.h"

/* ── qx_signal_snapshot_input ───────────────────────────────── */

/*
 * qx_signal_build_snapshot_input
 *
 * Builds a QXSnapshotInput from the current signal state.
 * Called by the constitutional cycle after measurement
 * to feed the intelligence snapshot layer.
 *
 * An [Angin] — the wind gathers all measurements and
 * presents them to the knowledge layer for recording.
 *
 * Law X.1 [Knowledge] — the system must know its state.
 * The snapshot input is how the system declares that
 * knowledge to the constitutional record.
 */
QXResult qx_signal_build_snapshot_input(
    QXMemlocSignalHandle          signal,
    const QXEngineABAMeasurement* aba,
    uint8_t                       pressure_tier,
    double                        law_health_score,
    uint32_t                      compliance_streak,
    double                        native_utilisation,
    QXTimestamp                   now_ms,
    QXSnapshotInput*              out_input)
{
    uint32_t s;
    QXSize   total_used = 0u;

    static const char* seg_ids[QX_POOL_SEGMENT_COUNT] = {
        "QLM-CORE", "QLM-UI",  "QLM-DATA",
        "QLM-IMG",  "QLM-NET", "QLM-AI",
        "QLM-SEC",  "QLM-LOG", "QLM-TEMP"
    };

    if (signal == NULL || !signal->is_initialised) {
        return QX_ERR_NULL_HANDLE;
    }
    if (aba == NULL || out_input == NULL) {
        return QX_ERR_NULL_HANDLE;
    }

    memset(out_input, 0, sizeof(QXSnapshotInput));

    /* ── Segment utilisation ────────────────────────────── */

    for (s = 0u; s < QX_POOL_SEGMENT_COUNT; ++s) {
        const QXPoolSegmentRegion* region =
            &signal->pool->segments[s];

        double hard_util = (region->hard_limit_bytes > 0u)
            ? ((double)region->committed_bytes /
               (double)region->hard_limit_bytes)
            : 0.0;

        strncpy(out_input->segment_ids[s],
                seg_ids[s],
                sizeof(out_input->segment_ids[s]) - 1u);

        out_input->segment_utilisations[s] = hard_util;
        total_used += region->committed_bytes;
    }

    out_input->segment_count = QX_POOL_SEGMENT_COUNT;

    /* ── Engine-level fields ────────────────────────────── */

    out_input->total_used_bytes        = total_used;
    out_input->composite_pressure_tier = pressure_tier;
    out_input->latest_law_health_score = law_health_score;
    out_input->compliance_streak       = compliance_streak;
    out_input->native_coverage_ratio   = native_utilisation;
    out_input->declared_capability_count = aba->total_leaves_measured;
    out_input->active_capability_count   = aba->compliant_leaves;
    out_input->captured_at_ms          = now_ms;

    return QX_OK;
}

/* ── qx_signal_cycle_stats ──────────────────────────────────── */

/*
 * qx_signal_cycle_stats
 *
 * Returns aggregate statistics about the signal layer's
 * measurement history. Used by telemetry providers
 * to emit knowledge score data to the law evaluator.
 *
 * An [Angin] — the wind remembers every measurement
 * it has taken. The history is the proof of knowledge.
 */
QXResult qx_signal_cycle_stats(
    QXMemlocSignalHandle signal,
    uint64_t*            out_total_cycles,
    QXTimestamp*         out_last_cycle_ms,
    uint32_t*            out_active_leaf_count)
{
    if (signal == NULL || !signal->is_initialised) {
        return QX_ERR_NULL_HANDLE;
    }

    pthread_mutex_lock(&signal->leaf_mutex);

    if (out_total_cycles != NULL) {
        *out_total_cycles = signal->total_cycles;
    }
    if (out_last_cycle_ms != NULL) {
        *out_last_cycle_ms = signal->last_cycle_ms;
    }
    if (out_active_leaf_count != NULL) {
        *out_active_leaf_count = signal->leaf_count;
    }

    pthread_mutex_unlock(&signal->leaf_mutex);
    return QX_OK;
}
