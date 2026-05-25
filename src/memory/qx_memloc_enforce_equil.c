/* ============================================================
 * qx_memloc_enforce_equil.c
 * QXMemloc — Ai [Api] — Equilibrium Enforcement
 *
 * Z.4 [Equilibrium] — all systems seek balance.
 * No segment may overflow while another starves.
 * The fire continuously monitors the balance of all
 * nine channels and restores equilibrium when needed.
 *
 * ALAMTOLOGI — Quranic Science
 * Founded by Masa Bayu
 *
 * Water naturally seeks its level.
 * When one field floods and another dries,
 * the irrigation system must rebalance the flow.
 * This is the Law of Z.4 made operational.
 *
 * Owner : Masa Bayu
 * Date  : 2026-05-25
 * Repo  : https://github.com/qxengine/qxengine-core
 * ============================================================ */

#include "qx_memloc_enforce_internal.h"

/* ── Internal: identify overloaded segments ─────────────────── */

/*
 * find_overloaded_segments
 *
 * Identifies segments exceeding the overload threshold.
 * An overloaded segment is taking more than its
 * constitutional share of the total pool.
 *
 * Z.4 [Equilibrium] — no channel should flood.
 * A flooded channel starves others downstream.
 */
static uint32_t find_overloaded_segments(
    const QXMemlocPool* pool,
    uint32_t*           out_indices,
    uint32_t            max_indices)
{
    uint32_t count = 0u;
    uint32_t i;

    for (i = 0u; i < QX_POOL_SEGMENT_COUNT &&
                 count < max_indices; ++i) {
        double util = enforce_segment_utilisation(
            pool, i
        );
        if (util > QX_ENFORCE_OVERLOAD_PCT) {
            out_indices[count++] = i;
        }
    }

    return count;
}

/* ── Internal: identify starved segments ────────────────────── */

/*
 * find_starved_segments
 *
 * Identifies segments below the starvation threshold
 * while at least one other segment is overloaded.
 *
 * Z.4 [Equilibrium] — starvation and overflow
 * together define constitutional imbalance.
 * One cannot exist constitutionally without the other
 * being a violation.
 */
static uint32_t find_starved_segments(
    const QXMemlocPool* pool,
    QXBool              any_overloaded,
    uint32_t*           out_indices,
    uint32_t            max_indices)
{
    uint32_t count = 0u;
    uint32_t i;

    if (!any_overloaded) {
        return 0u;
    }

    for (i = 0u; i < QX_POOL_SEGMENT_COUNT &&
                 count < max_indices; ++i) {
        double util = enforce_segment_utilisation(
            pool, i
        );
        if (util < QX_ENFORCE_STARVATION_PCT) {
            out_indices[count++] = i;
        }
    }

    return count;
}

/* ── Internal: reduce overloaded segment ───────────────────── */

/*
 * reduce_overloaded_segment
 *
 * Evicts class D then C leaves from an overloaded segment
 * until utilisation falls below the overload threshold
 * or there are no more evictable leaves.
 *
 * Ai [Api] — the fire drains the flooded channel.
 * Water returns from the flood to restore the balance.
 *
 * Must be called with flow->leaf_mutex LOCKED.
 */
static uint32_t reduce_overloaded_segment(
    struct QXMemlocFlow_s* flow,
    uint32_t               segment_idx,
    QXSize*                out_released)
{
    const char* seg_id;
    uint32_t    evicted = 0u;
    QXSize      rel     = 0u;

    seg_id = flow->pool->segments[segment_idx].segment_id;

    /* First try class D — least important */
    evicted += flow_evict_by_class(
        flow, QX_LEAF_CLASS_D, seg_id, &rel
    );
    *out_released += rel;

    /* If still overloaded try class C */
    if (enforce_segment_utilisation(
            flow->pool, segment_idx) >
        QX_ENFORCE_OVERLOAD_PCT) {
        rel = 0u;
        evicted += flow_evict_by_class(
            flow, QX_LEAF_CLASS_C, seg_id, &rel
        );
        *out_released += rel;
    }

    return evicted;
}

/* ── qx_enforce_equilibrium ─────────────────────────────────── */

QXResult qx_enforce_equilibrium(
    QXMemlocEnforceHandle  enforce,
    QXEnforcementResult*   out_result)
{
    struct QXMemlocFlow_s* flow;
    QXTimestamp            now;
    uint32_t               overloaded_idx[QX_POOL_SEGMENT_COUNT];
    uint32_t               starved_idx[QX_POOL_SEGMENT_COUNT];
    uint32_t               overloaded_count;
    uint32_t               starved_count;
    QXSize                 released = 0u;
    uint32_t               evicted  = 0u;
    uint32_t               i;
    QXBool                 restored = QX_FALSE;

    if (enforce == NULL || !enforce->is_initialised) {
        return QX_ERR_NULL_HANDLE;
    }
    if (out_result == NULL) {
        return QX_ERR_NULL_HANDLE;
    }

    memset(out_result, 0, sizeof(QXEnforcementResult));

    flow = enforce->flow;
    now  = enforce_now_ms();

    pthread_mutex_lock(&enforce->enforce_mutex);

    /* ── Identify imbalance ──────────────────────────────── */

    overloaded_count = find_overloaded_segments(
        flow->pool,
        overloaded_idx,
        QX_POOL_SEGMENT_COUNT
    );

    starved_count = find_starved_segments(
        flow->pool,
        (QXBool)(overloaded_count > 0u),
        starved_idx,
        QX_POOL_SEGMENT_COUNT
    );

    /*
     * Z.4 [Equilibrium] — imbalance requires both
     * overflow AND starvation simultaneously.
     * One segment overflowing alone is a Z.2 [Limit]
     * violation, not an equilibrium violation.
     * True equilibrium violation requires the asymmetry —
     * some field drowning while another field dries.
     */
    if (overloaded_count == 0u || starved_count == 0u) {
        /*
         * No imbalance detected.
         * The water is level. Z.4 is satisfied.
         */
        out_result->action                = QX_ACTION_NONE;
        out_result->equilibrium_restored  = QX_TRUE;
        out_result->enforced_at_ms        = now;
        pthread_mutex_unlock(&enforce->enforce_mutex);
        return QX_OK;
    }

    /* ── Drain overloaded segments ───────────────────────── */

    pthread_mutex_lock(&flow->leaf_mutex);

    for (i = 0u; i < overloaded_count; ++i) {
        QXSize seg_released = 0u;
        evicted += reduce_overloaded_segment(
            flow,
            overloaded_idx[i],
            &seg_released
        );
        released += seg_released;
    }

    pthread_mutex_unlock(&flow->leaf_mutex);

    /*
     * Check if equilibrium is restored.
     * After draining overloaded segments, verify that
     * no segment is still overloaded while another starves.
     * If restored — Z.4 is satisfied for this cycle.
     */
    overloaded_count = find_overloaded_segments(
        flow->pool,
        overloaded_idx,
        QX_POOL_SEGMENT_COUNT
    );
    starved_count = find_starved_segments(
        flow->pool,
        (QXBool)(overloaded_count > 0u),
        starved_idx,
        QX_POOL_SEGMENT_COUNT
    );

    restored = (QXBool)(
        overloaded_count == 0u || starved_count == 0u
    );

    /* ── Update statistics ──────────────────────────────── */

    enforce->total_evictions      += evicted;
    enforce->total_bytes_released += released;

    /* ── Populate result ────────────────────────────────── */

    out_result->action              = QX_ACTION_WARN_EQUILIBRIUM;
    out_result->evicted_count       = evicted;
    out_result->bytes_released      = released;
    out_result->pages_decommitted   =
        released / flow->pool->page_size;
    out_result->equilibrium_restored= restored;
    out_result->enforced_at_ms      = now;

    pthread_mutex_unlock(&enforce->enforce_mutex);
    return QX_OK;
}
