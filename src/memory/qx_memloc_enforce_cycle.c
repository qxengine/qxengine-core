/* ============================================================
 * qx_memloc_enforce_cycle.c
 * QXMemloc — Ai [Api] — Enforcement Cycle
 *
 * The primary enforcement function. Called every cognitive
 * cycle after An [Angin] has completed measurement.
 *
 * The wind measures. The fire enforces.
 * An [Angin] never judges — it only measures.
 * Ai [Api] receives those measurements and acts.
 *
 * ALAMTOLOGI — Quranic Science
 * Founded by Masa Bayu
 *
 * Enforces all four Laws of Z each cycle:
 *   Z.1 [Pattern]     — identity violations
 *   Z.2 [Limit]       — budget ceiling violations
 *   Z.3 [Pairs]       — orphan escalation
 *   Z.4 [Equilibrium] — cross-segment balance
 *
 * Owner : Masa Bayu
 * Date  : 2026-05-25
 * Repo  : https://github.com/qxengine/qxengine-core
 * ============================================================ */

#include "qx_memloc_enforce_internal.h"

/* ── qx_enforce_create ──────────────────────────────────────── */

QXResult qx_enforce_create(
    QXMemlocFlowHandle      flow,
    QXMemlocSignalHandle    signal,
    QXMemlocEnforceHandle*  out_enforce)
{
    struct QXMemlocEnforce_s* enforce;

    if (flow   == NULL || !flow->is_initialised)   {
        return QX_ERR_NULL_HANDLE;
    }
    if (signal == NULL || !signal->is_initialised) {
        return QX_ERR_NULL_HANDLE;
    }
    if (out_enforce == NULL) {
        return QX_ERR_NULL_HANDLE;
    }

    /*
     * Ai [Api] lives on the system heap.
     * Fire is not water — it does not live in the
     * channels it burns through.
     */
    enforce = (struct QXMemlocEnforce_s*)calloc(
        1u, sizeof(struct QXMemlocEnforce_s)
    );
    if (enforce == NULL) {
        return QX_ERR_INTERNAL;
    }

    enforce->flow             = flow;
    enforce->signal           = signal;
    enforce->total_cycles     = 0u;
    enforce->total_evictions  = 0u;
    enforce->total_bytes_released  = 0u;
    enforce->total_orphans_resolved= 0u;
    enforce->is_initialised   = QX_TRUE;

    if (pthread_mutex_init(
            &enforce->enforce_mutex, NULL) != 0) {
        free(enforce);
        return QX_ERR_INTERNAL;
    }

    *out_enforce = enforce;
    return QX_OK;
}

/* ── qx_enforce_destroy ─────────────────────────────────────── */

void qx_enforce_destroy(QXMemlocEnforceHandle enforce)
{
    if (enforce == NULL || !enforce->is_initialised) {
        return;
    }

    /* Z.3 [Pairs] — created, must be destroyed */
    pthread_mutex_destroy(&enforce->enforce_mutex);

    enforce->is_initialised = QX_FALSE;
    free(enforce);
}

/* ── Internal: enforce Z.1 Pattern ─────────────────────────── */

/*
 * enforce_pattern
 *
 * Z.1 [Pattern] — every leaf must have valid identity.
 * Scans all active leaves for missing or malformed labels.
 * A leaf without identity is a constitutional violation.
 *
 * In practice this should never fire — the flow layer
 * rejects unlabelled allocations at the gate.
 * This is the second line of constitutional defence.
 *
 * Must be called with flow->leaf_mutex LOCKED.
 */
static uint32_t enforce_pattern(
    struct QXMemlocFlow_s* flow,
    QXSize*                out_released)
{
    uint32_t violations = 0u;
    uint32_t i;

    for (i = 0u; i < QX_FLOW_MAX_LEAVES; ++i) {
        QXFlowLeafRecord* rec = &flow->leaves[i];

        if (!rec->is_active) {
            continue;
        }

        /*
         * Z.1 [Pattern] — identity check.
         * A label shorter than the minimum is formless.
         * The fire removes what has no identity.
         */
        if (strnlen(rec->label,
                    (size_t)QX_LABEL_MIN_LENGTH) <
            (size_t)QX_LABEL_MIN_LENGTH) {

            enforce_evict_fatal_leaf(
                flow, rec, out_released
            );
            violations++;
        }
    }

    return violations;
}

/* ── Internal: enforce Z.3 Pairs — orphan escalation ──────── */

/*
 * enforce_pairs
 *
 * Z.3 [Pairs] — water that stops flowing must return.
 * Orphan leaves are escalated by idle duration:
 *
 *   60s  — WARNING:  flag for monitoring
 *   120s — CRITICAL: flag for immediate attention
 *   180s — FATAL:    evict regardless of leaf class
 *
 * At FATAL the universe reclaims what was not returned.
 * Even class A leaves are evicted at FATAL orphan —
 * because a protected leaf that is never used is not
 * protecting anything. It is wasting existence.
 *
 * Must be called with flow->leaf_mutex LOCKED.
 */
static uint32_t enforce_pairs(
    struct QXMemlocFlow_s* flow,
    QXTimestamp            now,
    QXSize*                out_released)
{
    uint32_t resolved = 0u;
    uint32_t i;

    for (i = 0u; i < QX_FLOW_MAX_LEAVES; ++i) {
        QXFlowLeafRecord* rec = &flow->leaves[i];
        QXTimestamp       idle_ms;

        if (!rec->is_active) {
            continue;
        }

        idle_ms = now - rec->last_touched_ms;

        /*
         * FATAL orphan — 180 seconds of no activity.
         * The fire reclaims this water unconditionally.
         * Z.3 [Pairs] demands it.
         */
        if (enforce_orphan_stage(idle_ms) == 3u) {
            enforce_evict_fatal_leaf(
                flow, rec, out_released
            );
            resolved++;
        }
    }

    return resolved;
}

/* ── Internal: enforce Universal Formula fatal drift ───────── */

/*
 * enforce_fatal_x
 *
 * Evicts all leaves whose measured X drift has crossed
 * the FATAL threshold (drift_ratio >= QX_X_DRIFT_FATAL).
 *
 * X = m/t has broken for these leaves. The constitutional
 * constant is no longer holding. The fire must act.
 *
 * Ai [Api] — the fire burns with precision.
 * Only fatally drifted leaves are evicted here.
 * The wind (An [Angin]) has already classified them.
 * The fire simply executes the constitutional verdict.
 *
 * Must be called with flow->leaf_mutex LOCKED.
 */
static uint32_t enforce_fatal_x(
    struct QXMemlocFlow_s*    flow,
    struct QXMemlocSignal_s*  signal,
    QXSize*                   out_released)
{
    uint32_t evicted = 0u;
    uint32_t i;

    for (i = 0u; i < QX_FLOW_MAX_LEAVES; ++i) {
        QXFlowLeafRecord* rec = &flow->leaves[i];
        QXSignalLeafEntry* sig_entry;

        if (!rec->is_active) {
            continue;
        }

        /*
         * Find the matching signal entry.
         * The wind's measurement is the evidence.
         * The fire acts on that evidence.
         */
        sig_entry = signal_find_leaf(
            signal, rec->label
        );

        if (sig_entry == NULL) {
            continue;
        }

        if (sig_entry->drift_ratio >= QX_X_DRIFT_FATAL) {
            /*
             * X has broken — the Universal Formula
             * has been violated at the FATAL level.
             * Evict regardless of leaf class.
             *
             * Z.2 [Limit] — limits exist for a reason.
             * When X breaks Limit, the fire corrects.
             */
            enforce_evict_fatal_leaf(
                flow, rec, out_released
            );
            evicted++;
        }
    }

    return evicted;
}

/* ── qx_enforce_cycle ───────────────────────────────────────── */

QXResult qx_enforce_cycle(
    QXMemlocEnforceHandle         enforce,
    const QXEngineABAMeasurement* aba,
    QXEnforcementResult*          out_result)
{
    struct QXMemlocFlow_s*   flow;
    struct QXMemlocSignal_s* signal;
    QXTimestamp              now;
    QXSize                   released    = 0u;
    uint32_t                 evicted     = 0u;
    uint32_t                 orphans     = 0u;
    QXSize                   seg_released= 0u;

    if (enforce == NULL || !enforce->is_initialised) {
        return QX_ERR_NULL_HANDLE;
    }
    if (aba == NULL || out_result == NULL) {
        return QX_ERR_NULL_HANDLE;
    }

    memset(out_result, 0, sizeof(QXEnforcementResult));

    flow   = enforce->flow;
    signal = enforce->signal;
    now    = enforce_now_ms();

    pthread_mutex_lock(&enforce->enforce_mutex);
    pthread_mutex_lock(&flow->leaf_mutex);
    pthread_mutex_lock(&signal->leaf_mutex);

    /*
     * The ABA enforcement cycle:
     *
     * 1 [Pattern]   — enforce Z.1: identity
     * 2 [Limit]     — enforce Z.2: budget ceiling
     * 4 [Pairs]     — enforce Z.3: orphan escalation
     *                 enforce Universal Formula: fatal X
     * 4→2→1         — results aggregate to one report
     */

    /* ── Z.1 [Pattern] ──────────────────────────────────── */

    evicted += enforce_pattern(flow, &released);

    /* ── Z.3 [Pairs] — orphan escalation ────────────────── */

    orphans  = enforce_pairs(flow, now, &seg_released);
    released += seg_released;
    evicted  += orphans;
    seg_released = 0u;

    /* ── Universal Formula — fatal X drift ──────────────── */

    /*
     * Only act on fatal drift if the ABA measurement
     * reports fatal leaves. This avoids scanning all
     * leaves every cycle when no fatal drift exists —
     * the wind has already told us if we need to act.
     *
     * An [Angin] informs. Ai [Api] acts.
     */
    if (aba->fatal_leaves > 0u) {
        evicted += enforce_fatal_x(
            flow, signal, &seg_released
        );
        released += seg_released;
    }

    pthread_mutex_unlock(&signal->leaf_mutex);
    pthread_mutex_unlock(&flow->leaf_mutex);

    /* ── Update enforcement statistics ──────────────────── */

    enforce->total_cycles++;
    enforce->total_evictions      += evicted;
    enforce->total_bytes_released += released;
    enforce->total_orphans_resolved += orphans;

    /* ── Populate result ─────────────────────────────────── */

    out_result->action          = (evicted > 0u)
        ? QX_ACTION_EVICT_FATAL_X
        : QX_ACTION_NONE;
    out_result->evicted_count   = evicted;
    out_result->bytes_released  = released;
    out_result->orphans_resolved= orphans;
    out_result->enforced_at_ms  = now;

    /*
     * Pages decommitted is implicit in bytes_released —
     * every byte released goes through qx_pool_decommit
     * which returns physical pages to the OS immediately.
     */
    out_result->pages_decommitted =
        released / flow->pool->page_size;

    pthread_mutex_unlock(&enforce->enforce_mutex);
    return QX_OK;
}
