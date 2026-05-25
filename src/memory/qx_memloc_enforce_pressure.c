/* ============================================================
 * qx_memloc_enforce_pressure.c
 * QXMemloc — Ai [Api] — Pressure Response
 *
 * When the OS signals memory pressure the fire responds.
 * Each tier is a progressively stronger constitutional
 * response to the drought. The fire burns precisely —
 * removing what must be removed to preserve what must
 * be preserved.
 *
 * ALAMTOLOGI — Quranic Science
 * Founded by Masa Bayu
 *
 * Pressure cascade:
 *   Tier 1 (soft)        — D evicted,     70% budget
 *   Tier 2 (moderate)    — D+C evicted,   50% budget
 *   Tier 3 (critical)    — D+C+B evicted, 25% budget
 *   Tier 4 (imminentKill)— D+C+B evicted, 10% budget
 *   Class A — NEVER evicted. Constitutional protection.
 *
 * Owner : Masa Bayu
 * Date  : 2026-05-25
 * Repo  : https://github.com/qxengine/qxengine-core
 * ============================================================ */

#include "qx_memloc_enforce_internal.h"

/* ── Internal: determine eviction action ───────────────────── */

static QXEnforcementAction action_for_tier(uint8_t tier)
{
    switch (tier) {
        case 1u: return QX_ACTION_EVICT_SOFT;
        case 2u: return QX_ACTION_EVICT_MODERATE;
        case 3u: return QX_ACTION_EVICT_CRITICAL;
        case 4u: return QX_ACTION_EVICT_CRITICAL;
        default: return QX_ACTION_NONE;
    }
}

/* ── Internal: push reduced budgets after eviction ─────────── */

/*
 * push_pressure_budgets
 *
 * Fires budget callbacks for all segments with the
 * pressure-scaled limit. Subsystems must reduce their
 * actual usage to fit within the new narrower channels.
 *
 * Ar [Air] notified by Ai [Api].
 * The fire narrows the gates. The water adjusts.
 */
static void push_pressure_budgets(
    struct QXMemlocFlow_s* flow,
    uint8_t                pressure_tier,
    QXTimestamp            now)
{
    uint32_t i;

    for (i = 0u; i < QX_POOL_SEGMENT_COUNT; ++i) {
        const QXPoolSegmentRegion* region =
            &flow->pool->segments[i];

        double scale = flow_compute_pressure_scale(
            pressure_tier,
            flow->lifecycle_state
        );

        QXSize new_limit = (QXSize)(
            (double)region->soft_limit_bytes * scale
        );
        QXSize minimum_limit = enforce_min_viable_budget(
            region->soft_limit_bytes
        );

        if (pressure_tier == 4u && new_limit < minimum_limit) {
            new_limit = minimum_limit;
        }

        flow_fire_callbacks(
            flow,
            region->segment_id,
            new_limit,
            region->soft_limit_bytes,
            pressure_tier,
            flow->lifecycle_state,
            now
        );
    }
}

/* ── qx_enforce_pressure ────────────────────────────────────── */

QXResult qx_enforce_pressure(
    QXMemlocEnforceHandle  enforce,
    uint8_t                pressure_tier,
    QXEnforcementResult*   out_result)
{
    struct QXMemlocFlow_s* flow;
    QXTimestamp            now;
    QXSize                 released    = 0u;
    QXSize                 seg_rel     = 0u;
    uint32_t               evicted     = 0u;

    if (enforce == NULL || !enforce->is_initialised) {
        return QX_ERR_NULL_HANDLE;
    }
    if (pressure_tier == 0u || pressure_tier > 4u) {
        return QX_ERR_INVALID_ARGUMENT;
    }
    if (out_result == NULL) {
        return QX_ERR_NULL_HANDLE;
    }

    memset(out_result, 0, sizeof(QXEnforcementResult));

    flow = enforce->flow;
    now  = enforce_now_ms();

    pthread_mutex_lock(&enforce->enforce_mutex);
    pthread_mutex_lock(&flow->leaf_mutex);

    /*
     * Ai [Api] — the fire burns in constitutional order.
     *
     * The eviction cascade follows leaf class priority.
     * Lower classes burn first — they are the dry grass
     * at the edge of the field. Higher classes are the
     * roots — they survive longer.
     *
     * Class A roots never burn.
     * The universe protects what is constitutionally
     * declared as protected.
     *
     * Z.2 [Limit] — pressure enforces Limit.
     * The OS is telling us the universe has reached
     * its physical boundary. We respond with precision.
     */

    /* Tier 1+ — evict class D (backgroundProportional) */
    if (pressure_tier >= 1u) {
        seg_rel = 0u;
        evicted += flow_evict_by_class(
            flow, QX_LEAF_CLASS_D, NULL, &seg_rel
        );
        released += seg_rel;
    }

    /* Tier 2+ — evict class C (priorityProportional) */
    if (pressure_tier >= 2u) {
        seg_rel = 0u;
        evicted += flow_evict_by_class(
            flow, QX_LEAF_CLASS_C, NULL, &seg_rel
        );
        released += seg_rel;
    }

    /* Tier 3+ — evict class B (criticalFixed) */
    if (pressure_tier >= 3u) {
        seg_rel = 0u;
        evicted += flow_evict_by_class(
            flow, QX_LEAF_CLASS_B, NULL, &seg_rel
        );
        released += seg_rel;
    }

    /*
     * Class A (protectedFixed) — NEVER evicted.
     *
     * Not at tier 4. Not at imminentKill.
     * Not under any pressure.
     *
     * These allocations are constitutionally protected.
     * Auth state, core engine state, constitutional
     * infrastructure — they must survive.
     *
     * Z.1 [Pattern] — their identity is permanent.
     * The fire does not burn what the constitution
     * has declared inviolable.
     */

    pthread_mutex_unlock(&flow->leaf_mutex);

    /* Notify subsystems of reduced channel widths */
    push_pressure_budgets(flow, pressure_tier, now);

    /* ── Update statistics ──────────────────────────────── */

    enforce->total_evictions      += evicted;
    enforce->total_bytes_released += released;

    /* ── Populate result ────────────────────────────────── */

    out_result->action             = action_for_tier(
                                         pressure_tier);
    out_result->evicted_count      = evicted;
    out_result->bytes_released     = released;
    out_result->pages_decommitted  =
        released / flow->pool->page_size;
    out_result->orphans_resolved   = 0u;
    out_result->enforced_at_ms     = now;

    pthread_mutex_unlock(&enforce->enforce_mutex);
    return QX_OK;
}
