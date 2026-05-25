/* ============================================================
 * qx_memloc_flow_alloc.c
 * QXMemloc — Ar [Air] — Allocation and Deallocation
 *
 * Every drop of water enters and leaves through here.
 * Birth and return. The complete constitutional lifecycle
 * of one memory allocation.
 *
 * ALAMTOLOGI — Quranic Science
 * Founded by Masa Bayu
 *
 * Owner : Masa Bayu
 * Date  : 2026-05-25
 * Repo  : https://github.com/qxengine/qxengine-core
 * ============================================================ */

#include "qx_memloc_flow_internal.h"

/* ── Internal validation helpers ───────────────────────────── */

static QXResult validate_label(const char* label)
{
    size_t len;
    if (label == NULL) {
        return QX_ERR_LABEL_BLANK;
    }
    len = strnlen(label, (size_t)QX_LABEL_MAX_LENGTH + 1u);
    if (len == 0u) {
        return QX_ERR_LABEL_TOO_SHORT;
    }
    if (len < (size_t)QX_LABEL_MIN_LENGTH) {
        return QX_ERR_LABEL_TOO_SHORT;
    }
    if (len > (size_t)QX_LABEL_MAX_LENGTH) {
        return QX_ERR_LABEL_TOO_LONG;
    }
    return QX_OK;
}

static QXResult validate_segment(const char* segment_id)
{
    if (flow_find_segment_index(segment_id) ==
        QX_POOL_SEGMENT_COUNT) {
        return QX_ERR_UNKNOWN_SEGMENT;
    }
    return QX_OK;
}

static uint32_t find_free_slot(
    const struct QXMemlocFlow_s* flow)
{
    uint32_t i;
    for (i = 0u; i < QX_FLOW_MAX_LEAVES; ++i) {
        if (!flow->leaves[i].is_active) {
            return i;
        }
    }
    return QX_FLOW_MAX_LEAVES;
}

static QXFlowLeafRecord* find_leaf_slot_by_handle(
    struct QXMemlocFlow_s* flow,
    QXLeafHandle           leaf)
{
    uint32_t i;

    if (leaf == NULL) {
        return NULL;
    }

    for (i = 0u; i < QX_FLOW_MAX_LEAVES; ++i) {
        if ((QXLeafHandle)&flow->leaves[i] == leaf) {
            return &flow->leaves[i];
        }
    }

    return NULL;
}

/* ── qx_flow_allocate ───────────────────────────────────────── */

QXResult qx_flow_allocate(
    QXMemlocFlowHandle  flow,
    const char*         label,
    const char*         segment_id,
    QXLeafClassId       leaf_class,
    QXSize              size_bytes,
    QXFlowAllocation*   out_allocation)
{
    QXResult          r;
    uint32_t          slot_idx;
    uint32_t          seg_idx;
    QXSize            committed_size;
    QXFlowLeafRecord* rec;
    QXTimestamp       birth_ms;

    if (out_allocation == NULL) {
        return QX_ERR_NULL_HANDLE;
    }
    memset(out_allocation, 0, sizeof(QXFlowAllocation));

    if (flow == NULL || !flow->is_initialised) {
        return QX_ERR_NULL_HANDLE;
    }

    /* Z.1 [Pattern] — identity must exist */
    r = validate_label(label);
    if (r != QX_OK) {
        flow->unlabelled_rejections++;
        return r;
    }

    /* Z.1 [Pattern] — segment identity must be valid */
    r = validate_segment(segment_id);
    if (r != QX_OK) {
        return r;
    }

    /* Z.2 [Limit] — size must be non-zero */
    if (size_bytes == 0u) {
        return QX_ERR_INVALID_ARGUMENT;
    }
    if (leaf_class > QX_LEAF_CLASS_D) {
        return QX_ERR_INVALID_LEAF_CLASS;
    }

    seg_idx = flow_find_segment_index(segment_id);
    if (seg_idx == QX_POOL_SEGMENT_COUNT) {
        return QX_ERR_UNKNOWN_SEGMENT;
    }

    /* Z.2 [Limit] — round to slab class */
    committed_size = flow_slab_round_up(size_bytes);

    pthread_mutex_lock(&flow->leaf_mutex);

    slot_idx = find_free_slot(flow);
    if (slot_idx == QX_FLOW_MAX_LEAVES) {
        pthread_mutex_unlock(&flow->leaf_mutex);
        return QX_ERR_NO_SLOT;
    }

    /* Z.2 [Limit] — check soft budget */
    if (qx_pool_utilisation_pct(
            flow->pool, segment_id) >= 100.0) {
        pthread_mutex_unlock(&flow->leaf_mutex);
        return QX_ERR_SOFT_BUDGET;
    }

    /*
     * Ar [Air] — water flows into the channel.
     * Phase 2 commitment: physical pages activated.
     */
    r = qx_pool_commit(
        flow->pool, segment_id, committed_size
    );
    if (r != QX_OK) {
        pthread_mutex_unlock(&flow->leaf_mutex);
        return r;
    }

    /* m starts here — birth of X = m/t */
    birth_ms = flow_now_ms();

    rec = &flow->leaves[slot_idx];
    memset(rec, 0, sizeof(QXFlowLeafRecord));

    strncpy(rec->label, label,
            sizeof(rec->label) - 1u);
    strncpy(rec->segment_id, segment_id,
            sizeof(rec->segment_id) - 1u);

    rec->segment_idx     = seg_idx;
    rec->leaf_class      = leaf_class;
    rec->size_bytes      = size_bytes;
    rec->committed_bytes = committed_size;
    rec->allocated_at_ms = birth_ms;
    rec->last_touched_ms = birth_ms;
    rec->is_active       = QX_TRUE;
    rec->slot_index      = slot_idx;

    /*
     * Real pointer into the committed region.
     * The pointer IS the water arriving at the field.
     */
    rec->ptr = (void*)(
        flow->pool->segments[seg_idx].base +
        flow->pool->segments[seg_idx].committed_bytes -
        committed_size
    );

    /*
     * Leaf handle IS the constitutional identity.
     * Z.1 [Pattern] — without it the allocation
     * cannot be located, touched, or deallocated.
     */
    rec->leaf = (QXLeafHandle)rec;

    flow->total_allocations++;
    flow->total_bytes_allocated += committed_size;
    flow->leaf_count++;

    out_allocation->ptr               = rec->ptr;
    out_allocation->leaf              = rec->leaf;
    out_allocation->usable_bytes      = committed_size;
    out_allocation->allocated_at_ms   = birth_ms;
    out_allocation->pressure_tier     = flow->pressure_tier;
    out_allocation->evicted_count     = 0u;
    out_allocation->soft_pressure_pct =
        qx_pool_total_utilisation_pct(flow->pool);

    pthread_mutex_unlock(&flow->leaf_mutex);
    return QX_OK;
}

/* ── qx_flow_deallocate ─────────────────────────────────────── */

QXResult qx_flow_deallocate(
    QXMemlocFlowHandle  flow,
    QXLeafHandle        leaf,
    QXFlowRelease*      out_release)
{
    QXFlowLeafRecord* rec;
    QXTimestamp       release_ms;
    QXTimestamp       held_ms;
    double            t_normalised;
    double            final_x;
    QXSize            released_bytes;

    if (flow == NULL || !flow->is_initialised) {
        return QX_ERR_NULL_HANDLE;
    }
    if (leaf == NULL) {
        return QX_ERR_LEAF_NOT_FOUND;
    }
    if (out_release == NULL) {
        return QX_ERR_NULL_HANDLE;
    }

    memset(out_release, 0, sizeof(QXFlowRelease));

    pthread_mutex_lock(&flow->leaf_mutex);

    rec = find_leaf_slot_by_handle(flow, leaf);
    if (rec == NULL) {
        pthread_mutex_unlock(&flow->leaf_mutex);
        return QX_ERR_LEAF_NOT_FOUND;
    }

    /*
     * Z.3 [Pairs] — double-free detection.
     * Water cannot return twice to the same river.
     */
    if (!rec->is_active) {
        pthread_mutex_unlock(&flow->leaf_mutex);
        return QX_ERR_DOUBLE_FREE;
    }

    /*
     * Final X = m/t measurement.
     * The complete constitutional life of this allocation.
     * m = time held, t = bytes normalised.
     */
    release_ms   = flow_now_ms();
    held_ms = (release_ms > rec->allocated_at_ms)
        ? (release_ms - rec->allocated_at_ms)
        : 1u;
    t_normalised = (flow->pool->total_soft_limit_bytes > 0u)
        ? ((double)rec->committed_bytes /
           (double)flow->pool->total_soft_limit_bytes)
        : 1.0;
    final_x = (t_normalised > 0.0)
        ? ((double)held_ms / t_normalised)
        : 0.0;

    released_bytes = rec->committed_bytes;

    /*
     * Ar [Air] — water returns to the river.
     * Physical pages decommitted immediately.
     * Z.3 [Pairs] — every commit has its decommit.
     */
    qx_pool_decommit(
        flow->pool,
        rec->segment_id,
        rec->committed_bytes
    );

    rec->is_active = QX_FALSE;
    rec->ptr       = NULL;
    rec->leaf      = NULL;

    flow->total_deallocations++;
    flow->total_bytes_deallocated += released_bytes;
    if (flow->leaf_count > 0u) {
        flow->leaf_count--;
    }

    out_release->released_bytes    = released_bytes;
    out_release->released_at_ms    = release_ms;
    out_release->final_x           = final_x;
    out_release->soft_pressure_pct =
        qx_pool_total_utilisation_pct(flow->pool);
    out_release->pressure_tier     = flow->pressure_tier;

    pthread_mutex_unlock(&flow->leaf_mutex);
    return QX_OK;
}

/* ── qx_flow_touch ──────────────────────────────────────────── */

QXResult qx_flow_touch(
    QXMemlocFlowHandle flow,
    QXLeafHandle       leaf)
{
    QXFlowLeafRecord* rec;

    if (flow == NULL || !flow->is_initialised) {
        return QX_ERR_NULL_HANDLE;
    }
    if (leaf == NULL) {
        return QX_ERR_LEAF_NOT_FOUND;
    }

    pthread_mutex_lock(&flow->leaf_mutex);

    rec = flow_find_leaf_by_handle(flow, leaf);
    if (rec == NULL || !rec->is_active) {
        pthread_mutex_unlock(&flow->leaf_mutex);
        return QX_ERR_LEAF_NOT_FOUND;
    }

    /*
     * Z.3 [Pairs] — a touched leaf is alive and in use.
     * Resetting last_touched_ms resets the orphan timer.
     * Active water does not stagnate.
     */
    rec->last_touched_ms = flow_now_ms();

    pthread_mutex_unlock(&flow->leaf_mutex);
    return QX_OK;
}
