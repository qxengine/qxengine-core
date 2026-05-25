/* ============================================================
 * qx_memloc_pool_commit.c
 * QXMemloc — Th [Tanah] — Pool Commitment
 *
 * Owns page commitment and decommitment for pool segments.
 * The land remains fixed; physical water enters and leaves.
 *
 * ALAMTOLOGI — Quranic Science
 * Founded by Masa Bayu
 *
 * Owner : Masa Bayu
 * Date  : 2026-05-25
 * Repo  : https://github.com/qxengine/qxengine-core
 * ============================================================ */

#include "qx_memloc_pool_internal.h"

QXResult qx_pool_commit(
    QXMemlocPool* pool,
    const char*   segment_id,
    QXSize        additional_bytes)
{
    uint32_t             idx;
    QXPoolSegmentRegion* region;
    QXSize               aligned_bytes;
    QXSize               new_committed;
    int                  prot_result;

    if (pool == NULL || !pool->is_initialised) {
        return QX_ERR_NULL_HANDLE;
    }
    if (segment_id == NULL || additional_bytes == 0u) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    idx = qx_pool_find_segment_index(segment_id);
    if (idx == QX_POOL_SEGMENT_COUNT) {
        return QX_ERR_UNKNOWN_SEGMENT;
    }

    region = &pool->segments[idx];

    pthread_mutex_lock(&qx_pool_segment_mutexes[idx]);

    aligned_bytes = qx_pool_align_to_page(additional_bytes, pool->page_size);
    new_committed = region->committed_bytes + aligned_bytes;

    if (new_committed > region->hard_limit_bytes) {
        pthread_mutex_unlock(&qx_pool_segment_mutexes[idx]);
        return QX_ERR_HARD_BUDGET;
    }

    prot_result = mprotect(
        (void*)(region->base + region->committed_bytes),
        (size_t)aligned_bytes,
        PROT_READ | PROT_WRITE
    );

    if (prot_result != 0) {
        pthread_mutex_unlock(&qx_pool_segment_mutexes[idx]);
        return QX_ERR_INTERNAL;
    }

    region->committed_bytes = new_committed;
    if (new_committed > region->peak_committed) {
        region->peak_committed = new_committed;
    }

    pool->total_committed_bytes += aligned_bytes;

    pthread_mutex_unlock(&qx_pool_segment_mutexes[idx]);
    return QX_OK;
}

QXResult qx_pool_decommit(
    QXMemlocPool* pool,
    const char*   segment_id,
    QXSize        release_bytes)
{
    uint32_t             idx;
    QXPoolSegmentRegion* region;
    QXSize               aligned_bytes;
    QXSize               new_committed;
    uint8_t*             decommit_start;
    int                  prot_result;

    if (pool == NULL || !pool->is_initialised) {
        return QX_ERR_NULL_HANDLE;
    }
    if (segment_id == NULL || release_bytes == 0u) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    idx = qx_pool_find_segment_index(segment_id);
    if (idx == QX_POOL_SEGMENT_COUNT) {
        return QX_ERR_UNKNOWN_SEGMENT;
    }

    region = &pool->segments[idx];

    pthread_mutex_lock(&qx_pool_segment_mutexes[idx]);

    aligned_bytes = qx_pool_align_to_page(release_bytes, pool->page_size);
    if (aligned_bytes > region->committed_bytes) {
        aligned_bytes = region->committed_bytes;
    }

    if (aligned_bytes == 0u) {
        pthread_mutex_unlock(&qx_pool_segment_mutexes[idx]);
        return QX_OK;
    }

    new_committed  = region->committed_bytes - aligned_bytes;
    decommit_start = region->base + new_committed;

    prot_result = mprotect(
        (void*)decommit_start,
        (size_t)aligned_bytes,
        PROT_NONE
    );

    if (prot_result != 0) {
        pthread_mutex_unlock(&qx_pool_segment_mutexes[idx]);
        return QX_ERR_INTERNAL;
    }

    region->committed_bytes = new_committed;

    if (pool->total_committed_bytes >= aligned_bytes) {
        pool->total_committed_bytes -= aligned_bytes;
    } else {
        pool->total_committed_bytes = 0u;
    }

    pthread_mutex_unlock(&qx_pool_segment_mutexes[idx]);
    return QX_OK;
}
