/* ============================================================
 * qx_memloc_signal_registry.c
 * QXMemloc — An [Angin] — Leaf Registration
 *
 * The wind knows every leaf it must measure.
 * When a leaf is born the wind registers it.
 * When a leaf returns the wind releases it.
 *
 * ALAMTOLOGI — Quranic Science
 * Founded by Masa Bayu
 *
 * Law of Z.1 [Pattern]  — every leaf has identity
 * Law of Z.3 [Pairs]    — register pairs with deregister
 *
 * Owner : Masa Bayu
 * Date  : 2026-05-25
 * Repo  : https://github.com/qxengine/qxengine-core
 * ============================================================ */

#include "qx_memloc_signal_internal.h"

/* ── qx_signal_create ───────────────────────────────────────── */

QXResult qx_signal_create(
    const QXMemlocPool*    pool,
    double                 declared_x,
    double                 tolerance_pct,
    QXMemlocSignalHandle*  out_signal)
{
    struct QXMemlocSignal_s* signal;

    if (pool == NULL || !pool->is_initialised) {
        return QX_ERR_NULL_HANDLE;
    }
    if (out_signal == NULL) {
        return QX_ERR_NULL_HANDLE;
    }

    /*
     * Z.2 [Limit] — declared_x must be positive.
     * A zero or negative X is not a constitutional
     * constant — it is an undefined ratio.
     */
    if (declared_x <= 0.0) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    /*
     * Z.2 [Limit] — tolerance must be in range.
     * 0% = no drift allowed (impossible in practice).
     * 100% = any X is acceptable (no enforcement).
     * Constitutional range: 1% to 50%.
     */
    if (tolerance_pct < 1.0 || tolerance_pct > 50.0) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    /*
     * Signal layer lives on the system heap.
     * Like the flow controller — it is engine
     * infrastructure, not application memory.
     *
     * An [Angin] — the wind does not live in the
     * fields it measures. It moves through them.
     */
    signal = (struct QXMemlocSignal_s*)calloc(
        1u, sizeof(struct QXMemlocSignal_s)
    );
    if (signal == NULL) {
        return QX_ERR_INTERNAL;
    }

    signal->pool          = pool;
    signal->flow          = NULL;
    signal->declared_x    = declared_x;
    signal->tolerance_pct = tolerance_pct;
    signal->leaf_count    = 0u;
    signal->total_cycles  = 0u;
    signal->last_cycle_ms = 0u;
    signal->is_initialised= QX_TRUE;

    memset(signal->leaves, 0, sizeof(signal->leaves));

    if (pthread_mutex_init(&signal->leaf_mutex, NULL) != 0) {
        free(signal);
        return QX_ERR_INTERNAL;
    }

    *out_signal = signal;
    return QX_OK;
}

void qx_signal_attach_flow(
    QXMemlocSignalHandle signal,
    QXMemlocFlowHandle   flow)
{
    if (signal == NULL || !signal->is_initialised) {
        return;
    }
    signal->flow = flow;
}

void qx_signal_sync_from_flow(QXMemlocSignalHandle signal)
{
    uint32_t i;

    if (signal == NULL || !signal->is_initialised ||
        signal->flow == NULL || !signal->flow->is_initialised) {
        return;
    }

    pthread_mutex_lock(&signal->flow->leaf_mutex);
    pthread_mutex_lock(&signal->leaf_mutex);

    for (i = 0u; i < QX_FLOW_MAX_LEAVES; ++i) {
        const QXFlowLeafRecord* rec = &signal->flow->leaves[i];
        QXSignalLeafEntry* entry;
        uint32_t slot_idx;

        if (!rec->is_active) {
            continue;
        }

        entry = signal_find_leaf(signal, rec->label);
        if (entry != NULL) {
            continue;
        }

        slot_idx = signal_find_free_slot(signal);
        if (slot_idx == QX_SIGNAL_MAX_LEAVES) {
            break;
        }

        entry = &signal->leaves[slot_idx];
        memset(entry, 0, sizeof(QXSignalLeafEntry));

        strncpy(entry->leaf_id, rec->label,
                sizeof(entry->leaf_id) - 1u);
        strncpy(entry->segment_id, rec->segment_id,
                sizeof(entry->segment_id) - 1u);

        entry->allocated_at_ms   = rec->allocated_at_ms;
        entry->last_touched_ms   = rec->last_touched_ms;
        entry->reserved_bytes    = rec->committed_bytes;
        entry->declared_x        = signal->declared_x;
        entry->tolerance         = signal->tolerance_pct / 100.0;
        entry->is_active         = QX_TRUE;
        entry->is_orphan_candidate = QX_FALSE;

        signal->leaf_count++;
    }

    pthread_mutex_unlock(&signal->leaf_mutex);
    pthread_mutex_unlock(&signal->flow->leaf_mutex);
}

/* ── qx_signal_destroy ──────────────────────────────────────── */

void qx_signal_destroy(QXMemlocSignalHandle signal)
{
    if (signal == NULL || !signal->is_initialised) {
        return;
    }

    /*
     * Z.3 [Pairs] — the signal layer was created,
     * it must be destroyed. All tracked state is
     * released. The wind stops measuring.
     */
    pthread_mutex_destroy(&signal->leaf_mutex);

    signal->is_initialised = QX_FALSE;
    free(signal);
}

/* ── qx_signal_register_leaf ────────────────────────────────── */

QXResult qx_signal_register_leaf(
    QXMemlocSignalHandle signal,
    const char*          leaf_id,
    QXSize               reserved_bytes,
    QXTimestamp          allocated_at_ms)
{
    uint32_t           slot_idx;
    QXSignalLeafEntry* entry;

    if (signal == NULL || !signal->is_initialised) {
        return QX_ERR_NULL_HANDLE;
    }

    /*
     * Z.1 [Pattern] — leaf identity must exist.
     * The wind cannot measure what has no name.
     */
    if (leaf_id == NULL ||
        strnlen(leaf_id, 128u) == 0u) {
        return QX_ERR_LABEL_BLANK;
    }

    /* Z.2 [Limit] — reserved bytes must be non-zero */
    if (reserved_bytes == 0u) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&signal->leaf_mutex);

    /*
     * Z.1 [Pattern] — check for duplicate registration.
     * One leaf_id maps to exactly one signal entry.
     */
    if (signal_find_leaf(signal, leaf_id) != NULL) {
        pthread_mutex_unlock(&signal->leaf_mutex);
        return QX_ERR_INVALID_ARGUMENT;
    }

    slot_idx = signal_find_free_slot(signal);
    if (slot_idx == QX_SIGNAL_MAX_LEAVES) {
        pthread_mutex_unlock(&signal->leaf_mutex);
        return QX_ERR_NO_SLOT;
    }

    entry = &signal->leaves[slot_idx];
    memset(entry, 0, sizeof(QXSignalLeafEntry));

    strncpy(entry->leaf_id, leaf_id,
            sizeof(entry->leaf_id) - 1u);

    entry->allocated_at_ms   = allocated_at_ms;
    entry->last_touched_ms   = allocated_at_ms;
    entry->reserved_bytes    = reserved_bytes;
    entry->declared_x        = signal->declared_x;
    entry->tolerance         = signal->tolerance_pct / 100.0;
    entry->measured_x        = 0.0;
    entry->drift_ratio       = 0.0;
    entry->cumulative_x_sum  = 0.0;
    entry->measurement_count = 0u;
    entry->is_active         = QX_TRUE;
    entry->is_orphan_candidate = QX_FALSE;

    signal->leaf_count++;

    pthread_mutex_unlock(&signal->leaf_mutex);
    return QX_OK;
}

/* ── qx_signal_deregister_leaf ──────────────────────────────── */

QXResult qx_signal_deregister_leaf(
    QXMemlocSignalHandle    signal,
    const char*             leaf_id,
    QXLeafABAMeasurement*   out_final_measurement)
{
    QXSignalLeafEntry* entry;
    QXTimestamp        now;
    double             final_x;
    double             final_drift;
    double             tolerance;

    if (signal == NULL || !signal->is_initialised) {
        return QX_ERR_NULL_HANDLE;
    }
    if (leaf_id == NULL) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&signal->leaf_mutex);

    entry = signal_find_leaf(signal, leaf_id);
    if (entry == NULL) {
        pthread_mutex_unlock(&signal->leaf_mutex);
        qx_signal_sync_from_flow(signal);
        pthread_mutex_lock(&signal->leaf_mutex);
        entry = signal_find_leaf(signal, leaf_id);
    }
    if (entry == NULL) {
        pthread_mutex_unlock(&signal->leaf_mutex);
        return QX_ERR_LEAF_NOT_FOUND;
    }

    /*
     * Compute final X measurement before releasing.
     * This is the last breath of this leaf —
     * the wind records the complete constitutional
     * life of this drop of water.
     *
     * X = m/t — final measurement.
     */
    now        = signal_now_ms();
    final_x    = signal_compute_x(
        entry, now,
        signal->pool->total_soft_limit_bytes
    );
    final_drift = signal_compute_drift(
        final_x, entry->declared_x
    );
    tolerance = entry->tolerance;

    if (out_final_measurement != NULL) {
        memset(out_final_measurement, 0,
               sizeof(QXLeafABAMeasurement));

        strncpy(out_final_measurement->leaf_id,
                leaf_id,
                sizeof(out_final_measurement->leaf_id) - 1u);

        out_final_measurement->declared_x      = entry->declared_x;
        out_final_measurement->measured_x      = final_x;
        out_final_measurement->drift_ratio     = final_drift;
        out_final_measurement->is_constant     =
            (QXBool)(final_drift <= tolerance);
        out_final_measurement->is_orphan       =
            entry->is_orphan_candidate;
        out_final_measurement->measured_at_ms  = now;
    }

    /* Release the slot — Z.3 [Pairs] */
    memset(entry, 0, sizeof(QXSignalLeafEntry));

    if (signal->leaf_count > 0u) {
        signal->leaf_count--;
    }

    pthread_mutex_unlock(&signal->leaf_mutex);
    return QX_OK;
}

/* ── qx_signal_touch_leaf ───────────────────────────────────── */

QXResult qx_signal_touch_leaf(
    QXMemlocSignalHandle signal,
    const char*          leaf_id,
    QXTimestamp          touched_at_ms)
{
    QXSignalLeafEntry* entry;

    if (signal == NULL || !signal->is_initialised) {
        return QX_ERR_NULL_HANDLE;
    }
    if (leaf_id == NULL) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&signal->leaf_mutex);

    entry = signal_find_leaf(signal, leaf_id);
    if (entry == NULL) {
        pthread_mutex_unlock(&signal->leaf_mutex);
        qx_signal_sync_from_flow(signal);
        pthread_mutex_lock(&signal->leaf_mutex);
        entry = signal_find_leaf(signal, leaf_id);
    }
    if (entry == NULL) {
        pthread_mutex_unlock(&signal->leaf_mutex);
        return QX_ERR_LEAF_NOT_FOUND;
    }

    /*
     * Z.3 [Pairs] — a touched leaf is alive.
     * The orphan timer resets. Active water does
     * not stagnate. The wind records the activity.
     */
    entry->last_touched_ms    = touched_at_ms;
    entry->is_orphan_candidate= QX_FALSE;

    pthread_mutex_unlock(&signal->leaf_mutex);
    return QX_OK;
}

/* ── qx_signal_get_leaf_state ───────────────────────────────── */

QXResult qx_signal_get_leaf_state(
    QXMemlocSignalHandle signal,
    const char*          leaf_id,
    QXLeafXState*        out_state)
{
    QXSignalLeafEntry* entry;

    if (signal == NULL || !signal->is_initialised) {
        return QX_ERR_NULL_HANDLE;
    }
    if (leaf_id == NULL || out_state == NULL) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    memset(out_state, 0, sizeof(QXLeafXState));

    pthread_mutex_lock(&signal->leaf_mutex);

    entry = signal_find_leaf(signal, leaf_id);
    if (entry == NULL) {
        pthread_mutex_unlock(&signal->leaf_mutex);
        qx_signal_sync_from_flow(signal);
        pthread_mutex_lock(&signal->leaf_mutex);
        entry = signal_find_leaf(signal, leaf_id);
    }
    if (entry == NULL) {
        pthread_mutex_unlock(&signal->leaf_mutex);
        return QX_ERR_LEAF_NOT_FOUND;
    }

    strncpy(out_state->leaf_id, entry->leaf_id,
            sizeof(out_state->leaf_id) - 1u);

    out_state->allocated_at_ms    = entry->allocated_at_ms;
    out_state->last_touched_ms    = entry->last_touched_ms;
    out_state->reserved_bytes     = entry->reserved_bytes;
    out_state->declared_x         = entry->declared_x;
    out_state->measured_x         = entry->measured_x;
    out_state->drift_ratio        = entry->drift_ratio;
    out_state->measurement_count  = entry->measurement_count;
    out_state->is_orphan_candidate= entry->is_orphan_candidate;

    if (entry->measurement_count > 0u) {
        out_state->cumulative_x =
            entry->cumulative_x_sum /
            (double)entry->measurement_count;
    }

    pthread_mutex_unlock(&signal->leaf_mutex);
    return QX_OK;
}
