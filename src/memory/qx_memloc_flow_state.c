/* ============================================================
 * qx_memloc_flow_state.c
 * QXMemloc — Ar [Air] — Flow Controller Lifecycle
 *
 * Creates and destroys the flow controller.
 * The irrigation system itself — not the water,
 * not the fields, but the channels and gates.
 *
 * ALAMTOLOGI — Quranic Science
 * Founded by Masa Bayu
 *
 * Owner : Masa Bayu
 * Date  : 2026-05-25
 * Repo  : https://github.com/qxengine/qxengine-core
 * ============================================================ */

#include "qx_memloc_flow_internal.h"

/* ── Internal: evict leaves by class ───────────────────────── */

/*
 * flow_evict_by_class
 *
 * Evicts all active leaves of a given class.
 * Decommits their physical pages immediately.
 *
 * Ai [Api] — fire burns away what must be released.
 * Z.3 [Pairs] — evicted leaves are properly retired.
 *
 * Must be called with leaf_mutex LOCKED.
 */
uint32_t flow_evict_by_class(
    struct QXMemlocFlow_s* flow,
    QXLeafClassId          evict_class,
    const char*            segment_id,
    QXSize*                out_bytes_released)
{
    uint32_t evicted  = 0u;
    QXSize   released = 0u;
    uint32_t i;

    for (i = 0u; i < QX_FLOW_MAX_LEAVES; ++i) {
        QXFlowLeafRecord* rec = &flow->leaves[i];

        if (!rec->is_active) {
            continue;
        }
        if (segment_id != NULL &&
            strncmp(rec->segment_id,
                    segment_id, 16u) != 0) {
            continue;
        }
        if (rec->leaf_class != evict_class) {
            continue;
        }

        /*
         * Decommit physical pages.
         * Ar [Air] — water drains from this channel.
         * Physical RAM returns to the OS immediately.
         */
        qx_pool_decommit(
            flow->pool,
            rec->segment_id,
            rec->committed_bytes
        );

        released += rec->committed_bytes;

        rec->is_active = QX_FALSE;
        rec->ptr       = NULL;
        rec->leaf      = NULL;

        flow->total_deallocations++;
        evicted++;
    }

    if (out_bytes_released != NULL) {
        *out_bytes_released = released;
    }
    return evicted;
}

/* ── Public API ─────────────────────────────────────────────── */

QXResult qx_flow_create(
    QXMemlocPool*       pool,
    QXMemlocFlowHandle* out_flow)
{
    struct QXMemlocFlow_s* flow;

    if (pool == NULL || !pool->is_initialised) {
        return QX_ERR_NULL_HANDLE;
    }
    if (out_flow == NULL) {
        return QX_ERR_NULL_HANDLE;
    }

    /*
     * The flow controller is engine infrastructure.
     * It lives on the system heap — not in the pool.
     * The pool is for application memory only.
     *
     * Th [Tanah] — the irrigation system is built
     * from different material than the water it carries.
     */
    flow = (struct QXMemlocFlow_s*)calloc(
        1u, sizeof(struct QXMemlocFlow_s)
    );
    if (flow == NULL) {
        return QX_ERR_INTERNAL;
    }

    flow->pool              = pool;
    flow->leaf_count        = 0u;
    flow->callback_count    = 0u;
    flow->usage_report_count= 0u;
    flow->lifecycle_state   = QX_LIFECYCLE_FOREGROUND;
    flow->pressure_tier     = 0u;
    flow->is_initialised    = QX_TRUE;

    /* Initialise mutexes — Z.3 [Pairs] */
    if (pthread_mutex_init(&flow->leaf_mutex,     NULL) != 0 ||
        pthread_mutex_init(&flow->callback_mutex, NULL) != 0 ||
        pthread_mutex_init(&flow->usage_mutex,    NULL) != 0) {
        pthread_mutex_destroy(&flow->leaf_mutex);
        pthread_mutex_destroy(&flow->callback_mutex);
        pthread_mutex_destroy(&flow->usage_mutex);
        free(flow);
        return QX_ERR_INTERNAL;
    }

    *out_flow = flow;
    return QX_OK;
}

void qx_flow_destroy(QXMemlocFlowHandle flow)
{
    uint32_t i;

    if (flow == NULL || !flow->is_initialised) {
        return;
    }

    /*
     * Z.3 [Pairs] — all active leaves must be retired.
     * Every drop of water must return to the river.
     * We do not silently abandon active allocations.
     */
    pthread_mutex_lock(&flow->leaf_mutex);

    for (i = 0u; i < QX_FLOW_MAX_LEAVES; ++i) {
        QXFlowLeafRecord* rec = &flow->leaves[i];
        if (rec->is_active && rec->committed_bytes > 0u) {
            qx_pool_decommit(
                flow->pool,
                rec->segment_id,
                rec->committed_bytes
            );
            rec->is_active = QX_FALSE;
        }
    }

    pthread_mutex_unlock(&flow->leaf_mutex);

    /* Destroy mutexes — Z.3 [Pairs] */
    pthread_mutex_destroy(&flow->usage_mutex);
    pthread_mutex_destroy(&flow->callback_mutex);
    pthread_mutex_destroy(&flow->leaf_mutex);

    flow->is_initialised = QX_FALSE;
    free(flow);
}
