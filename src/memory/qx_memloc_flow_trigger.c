/* ============================================================
 * qx_memloc_flow_trigger.c
 * QXMemloc — Ar [Air] — Recalculation Triggers
 *
 * The events that redirect the water flow.
 * Lifecycle changes narrow or widen all channels.
 * Screen profile changes redirect flow between segments.
 * Pressure events narrow channels and evict leaves.
 *
 * ALAMTOLOGI — Quranic Science
 * Founded by Masa Bayu
 *
 * Owner : Masa Bayu
 * Date  : 2026-05-25
 * Repo  : https://github.com/qxengine/qxengine-core
 * ============================================================ */

#include "qx_memloc_flow_internal.h"

/* ── Internal: push new limits to all segments ──────────────── */

static void push_all_segment_budgets(
    struct QXMemlocFlow_s* flow,
    uint8_t                pressure_tier,
    uint8_t                lifecycle_state,
    QXTimestamp            ts)
{
    uint32_t i;

    for (i = 0u; i < QX_POOL_SEGMENT_COUNT; ++i) {
        const QXPoolSegmentRegion* region =
            &flow->pool->segments[i];

        double scale = flow_compute_pressure_scale(
            pressure_tier, lifecycle_state
        );

        QXSize new_limit = (QXSize)(
            (double)region->soft_limit_bytes * scale
        );

        flow_fire_callbacks(
            flow,
            region->segment_id,
            new_limit,
            region->soft_limit_bytes,
            pressure_tier,
            lifecycle_state,
            ts
        );
    }
}

/* ── qx_flow_notify_lifecycle_change ────────────────────────── */

QXResult qx_flow_notify_lifecycle_change(
    QXMemlocFlowHandle flow,
    uint8_t            lifecycle_state)
{
    if (flow == NULL || !flow->is_initialised) {
        return QX_ERR_NULL_HANDLE;
    }
    if (lifecycle_state > QX_LIFECYCLE_BACKGROUND) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    flow->lifecycle_state = lifecycle_state;

    /*
     * Ar [Air] — the season changes.
     * Background is winter — channels narrow to 25%.
     * Foreground is summer — channels open fully.
     *
     * Every registered field is notified of the
     * new water allocation for this season.
     */
    push_all_segment_budgets(
        flow,
        flow->pressure_tier,
        lifecycle_state,
        flow_now_ms()
    );

    return QX_OK;
}

/* ── qx_flow_notify_screen_profile_change ───────────────────── */

QXResult qx_flow_notify_screen_profile_change(
    QXMemlocFlowHandle flow,
    const char*        screen_profile_id)
{
    if (flow == NULL || !flow->is_initialised) {
        return QX_ERR_NULL_HANDLE;
    }
    if (screen_profile_id == NULL) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    strncpy(flow->screen_profile_id,
            screen_profile_id,
            sizeof(flow->screen_profile_id) - 1u);
    flow->screen_profile_id[
        sizeof(flow->screen_profile_id) - 1u] = '\0';

    /*
     * Ar [Air] — the fields change.
     * Moving from homeCatalog to arenaLive redirects
     * water from IMG and LOC toward AV and NET.
     *
     * Profile-specific budget tables are resolved
     * at the QIUBBX manifest layer. QXMemloc fires
     * all callbacks so each subsystem can re-evaluate
     * its allocation against the new profile context.
     */
    push_all_segment_budgets(
        flow,
        flow->pressure_tier,
        flow->lifecycle_state,
        flow_now_ms()
    );

    return QX_OK;
}

/* ── qx_flow_notify_pressure_change ─────────────────────────── */

QXResult qx_flow_notify_pressure_change(
    QXMemlocFlowHandle flow,
    uint8_t            pressure_tier)
{
    QXSize   seg_released   = 0u;

    if (flow == NULL || !flow->is_initialised) {
        return QX_ERR_NULL_HANDLE;
    }
    if (pressure_tier > 4u) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    flow->pressure_tier = pressure_tier;

    /*
     * Ai [Api] — pressure fires.
     * The drought has come. Channels must narrow.
     * Water returns from fields that can spare it
     * to preserve fields that cannot go dry.
     *
     * Eviction order — constitutional priority:
     *   Tier 1: D only  (backgroundProportional)
     *   Tier 2: D + C   (+ priorityProportional)
     *   Tier 3: D + C + B (+ criticalFixed)
     *   Tier 4: D + C + B (protectedFixed never evicted)
     *
     * Z.1 [Pattern] — class A leaves have permanent
     * identity. They are never evicted. The universe
     * does not destroy what is constitutionally protected.
     */
    pthread_mutex_lock(&flow->leaf_mutex);

    if (pressure_tier >= 1u) {
        flow_evict_by_class(
            flow, QX_LEAF_CLASS_D,
            NULL, &seg_released
        );
    }
    if (pressure_tier >= 2u) {
        flow_evict_by_class(
            flow, QX_LEAF_CLASS_C,
            NULL, &seg_released
        );
    }
    if (pressure_tier >= 3u) {
        flow_evict_by_class(
            flow, QX_LEAF_CLASS_B,
            NULL, &seg_released
        );
    }

    pthread_mutex_unlock(&flow->leaf_mutex);

    /*
     * Notify all subsystems of reduced channel widths.
     * Ar [Air] — the gate narrows.
     * Each field receives its drought allocation.
     */
    push_all_segment_budgets(
        flow,
        pressure_tier,
        flow->lifecycle_state,
        flow_now_ms()
    );

    return QX_OK;
}
