/* ============================================================
 * qx_memloc_enforce_restore.c
 * QXMemloc — Ai [Api] — Recovery After Pressure
 *
 * When pressure recedes the fire does not keep burning.
 * Constitutional fire is precise — it burns what must
 * burn and stops when the constitution is restored.
 *
 * ALAMTOLOGI — Quranic Science
 * Founded by Masa Bayu
 *
 * Recovery follows the same ABA form as pressure:
 *   4→2→1 contraction restores to one constitutional
 *   equilibrium — just as it does in nature.
 *
 * When the drought ends:
 *   — Budgets are restored to their constitutional limits
 *   — Subsystems are notified of expanded channels
 *   — The water flows again at the correct rate
 *
 * Z.3 [Pairs] — pressure has a recovery.
 * Z.4 [Equilibrium] — the system returns to balance.
 * X = m/t — the constitutional constant is restored.
 *
 * Owner : Masa Bayu
 * Date  : 2026-05-25
 * Repo  : https://github.com/qxengine/qxengine-core
 * ============================================================ */

#include "qx_memloc_enforce_internal.h"

/* ── Internal: compute restored limit ──────────────────────── */

/*
 * compute_restored_limit
 *
 * Computes the restored budget limit for a segment
 * after pressure tier reduction.
 *
 * The restored limit is the soft_limit_bytes scaled by
 * the new (lower) pressure tier. If the new tier is 0
 * (no pressure) the full soft limit is restored.
 *
 * Z.3 [Pairs] — the reduction has its restoration.
 * Z.4 [Equilibrium] — budgets return proportionally.
 */
static QXSize compute_restored_limit(
    QXSize  soft_limit_bytes,
    uint8_t new_tier,
    uint8_t lifecycle_state)
{
    double scale;

    if (new_tier == 0u) {
        /*
         * No pressure — full constitutional budget
         * restored. The drought is over. The river
         * flows at its natural rate again.
         */
        scale = (lifecycle_state == 1u) ? 0.25 : 1.0;
    } else {
        scale = flow_compute_pressure_scale(
            new_tier, lifecycle_state
        );
    }

    return (QXSize)((double)soft_limit_bytes * scale);
}

/* ── qx_enforce_restore ─────────────────────────────────────── */

QXResult qx_enforce_restore(
    QXMemlocEnforceHandle  enforce,
    uint8_t                previous_tier,
    uint8_t                current_tier,
    QXEnforcementResult*   out_result)
{
    struct QXMemlocFlow_s* flow;
    QXTimestamp            now;
    uint32_t               i;

    if (enforce == NULL || !enforce->is_initialised) {
        return QX_ERR_NULL_HANDLE;
    }
    if (out_result == NULL) {
        return QX_ERR_NULL_HANDLE;
    }

    /*
     * Z.3 [Pairs] — restoration only makes sense
     * when the new tier is lower than the previous.
     * Calling restore when tier is increasing is a
     * logic error — the fire should be burning,
     * not extinguishing.
     */
    if (current_tier >= previous_tier) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    memset(out_result, 0, sizeof(QXEnforcementResult));

    flow = enforce->flow;
    now  = enforce_now_ms();

    pthread_mutex_lock(&enforce->enforce_mutex);

    flow->pressure_tier = current_tier;

    /*
     * Notify all subsystems of expanded channel widths.
     *
     * Ar [Air] — the drought is easing.
     * The gates reopen proportionally to the new tier.
     * Subsystems receive their restored allocations.
     *
     * The water flows back into the channels.
     * Not all at once — proportionally, constitutionally,
     * according to the new tier's scale factor.
     *
     * This is the 4→2→1 return of the ABA form:
     *   Four pressure tiers → two directions →
     *   one constitutional equilibrium.
     */
    for (i = 0u; i < QX_POOL_SEGMENT_COUNT; ++i) {
        const QXPoolSegmentRegion* region =
            &flow->pool->segments[i];

        QXSize previous_limit = compute_restored_limit(
            region->soft_limit_bytes,
            previous_tier,
            flow->lifecycle_state
        );

        QXSize new_limit = compute_restored_limit(
            region->soft_limit_bytes,
            current_tier,
            flow->lifecycle_state
        );

        /*
         * Only notify if the limit has actually increased.
         * A restore that produces the same or smaller limit
         * is not a recovery — it is a continued constraint.
         *
         * Z.1 [Pattern] — only meaningful signals
         * are sent. The wind does not blow for nothing.
         */
        if (new_limit > previous_limit) {
            flow_fire_callbacks(
                flow,
                region->segment_id,
                new_limit,
                previous_limit,
                current_tier,
                flow->lifecycle_state,
                now
            );
        }
    }

    /* ── Populate result ────────────────────────────────── */

    out_result->action              = QX_ACTION_RESTORE_BUDGET;
    out_result->evicted_count       = 0u;
    out_result->bytes_released      = 0u;
    out_result->pages_decommitted   = 0u;
    out_result->orphans_resolved    = 0u;
    out_result->equilibrium_restored= QX_TRUE;
    out_result->enforced_at_ms      = now;

    pthread_mutex_unlock(&enforce->enforce_mutex);
    return QX_OK;
}
