/* ============================================================
 * qx_memloc_pool_query.c
 * QXMemloc — Th [Tanah] — Pool Queries
 *
 * Read-only accessors for segment boundaries and utilisation.
 *
 * ALAMTOLOGI — Quranic Science
 * Founded by Masa Bayu
 *
 * Owner : Masa Bayu
 * Date  : 2026-05-25
 * Repo  : https://github.com/qxengine/qxengine-core
 * ============================================================ */

#include "qx_memloc_pool_internal.h"

const QXPoolSegmentRegion* qx_pool_segment_region(
    const QXMemlocPool* pool,
    const char*         segment_id)
{
    uint32_t idx;

    if (pool == NULL || !pool->is_initialised || segment_id == NULL) {
        return NULL;
    }

    idx = qx_pool_find_segment_index(segment_id);
    if (idx == QX_POOL_SEGMENT_COUNT) {
        return NULL;
    }

    return &pool->segments[idx];
}

double qx_pool_utilisation_pct(
    const QXMemlocPool* pool,
    const char*         segment_id)
{
    uint32_t                   idx;
    const QXPoolSegmentRegion* region;

    if (pool == NULL || !pool->is_initialised || segment_id == NULL) {
        return 0.0;
    }

    idx = qx_pool_find_segment_index(segment_id);
    if (idx == QX_POOL_SEGMENT_COUNT) {
        return 0.0;
    }

    region = &pool->segments[idx];
    if (region->soft_limit_bytes == 0u) {
        return 0.0;
    }

    return ((double)region->committed_bytes /
            (double)region->soft_limit_bytes) * 100.0;
}

double qx_pool_total_utilisation_pct(const QXMemlocPool* pool)
{
    if (pool == NULL || !pool->is_initialised) {
        return 0.0;
    }

    if (pool->total_soft_limit_bytes == 0u) {
        return 0.0;
    }

    return ((double)pool->total_committed_bytes /
            (double)pool->total_soft_limit_bytes) * 100.0;
}
