/* ============================================================
 * qx_memloc_pool.c
 * QXMemloc — Th [Tanah] — Pool Lifecycle
 *
 * The earth beneath the water. Fixed. Constitutional.
 * Owns pool creation, destruction, and shared helpers.
 *
 * ALAMTOLOGI — Quranic Science
 * Founded by Masa Bayu
 *
 * Owner : Masa Bayu
 * Date  : 2026-05-25
 * Repo  : https://github.com/qxengine/qxengine-core
 * ============================================================ */

#include "qx_memloc_pool_internal.h"

/* ── Internal constants ─────────────────────────────────────── */

/*
 * Proportional weight of each segment within the total pool.
 * Weights sum to 1.0. Derived from constitutional priorities.
 */
const double qx_pool_segment_weights[QX_POOL_SEGMENT_COUNT] = {
    0.10,  /* QLM-CORE  */
    0.12,  /* QLM-UI    */
    0.08,  /* QLM-DATA  */
    0.20,  /* QLM-IMG   */
    0.08,  /* QLM-NET   */
    0.10,  /* QLM-AI    */
    0.06,  /* QLM-SEC   */
    0.06,  /* QLM-LOG   */
    0.20   /* QLM-TEMP  */
};

const char* const qx_pool_segment_ids[QX_POOL_SEGMENT_COUNT] = {
    "QLM-CORE",
    "QLM-UI",
    "QLM-DATA",
    "QLM-IMG",
    "QLM-NET",
    "QLM-AI",
    "QLM-SEC",
    "QLM-LOG",
    "QLM-TEMP"
};

pthread_mutex_t qx_pool_segment_mutexes[QX_POOL_SEGMENT_COUNT];
int             qx_pool_mutexes_initialised = 0;

/* ── Shared helpers ─────────────────────────────────────────── */

QXSize qx_pool_platform_page_size(void)
{
    return (QXSize)getpagesize();
}

QXSize qx_pool_align_to_page(QXSize size_bytes, QXSize page_size)
{
    if (size_bytes == 0u) {
        return page_size;
    }
    return ((size_bytes + page_size - 1u) / page_size) * page_size;
}

uint32_t qx_pool_find_segment_index(const char* segment_id)
{
    uint32_t i;
    if (segment_id == NULL) {
        return QX_POOL_SEGMENT_COUNT;
    }
    for (i = 0u; i < QX_POOL_SEGMENT_COUNT; ++i) {
        if (strncmp(segment_id, qx_pool_segment_ids[i], 16u) == 0) {
            return i;
        }
    }
    return QX_POOL_SEGMENT_COUNT;
}

QXResult qx_pool_init_segment_mutexes(void)
{
    uint32_t i;
    uint32_t initialised_count = 0u;

    if (qx_pool_mutexes_initialised != 0) {
        return QX_OK;
    }

    for (i = 0u; i < QX_POOL_SEGMENT_COUNT; ++i) {
        if (pthread_mutex_init(&qx_pool_segment_mutexes[i], NULL) != 0) {
            uint32_t j;
            for (j = 0u; j < initialised_count; ++j) {
                pthread_mutex_destroy(&qx_pool_segment_mutexes[j]);
            }
            return QX_ERR_INTERNAL;
        }
        ++initialised_count;
    }

    qx_pool_mutexes_initialised = 1;
    return QX_OK;
}

void qx_pool_destroy_segment_mutexes(void)
{
    uint32_t i;
    if (qx_pool_mutexes_initialised == 0) {
        return;
    }
    for (i = 0u; i < QX_POOL_SEGMENT_COUNT; ++i) {
        pthread_mutex_destroy(&qx_pool_segment_mutexes[i]);
    }
    qx_pool_mutexes_initialised = 0;
}

QXResult qx_pool_validate_pool_limits(
    QXSize soft_limit_bytes,
    QXSize hard_limit_bytes)
{
    if (soft_limit_bytes < QX_POOL_MIN_BYTES) {
        return QX_ERR_INVALID_ARGUMENT;
    }
    if (hard_limit_bytes < soft_limit_bytes) {
        return QX_ERR_INVALID_ARGUMENT;
    }
    if (hard_limit_bytes > QX_POOL_MAX_BYTES) {
        return QX_ERR_INVALID_ARGUMENT;
    }
    return QX_OK;
}

QXResult qx_pool_validate_device_scale(double device_scale)
{
    if (device_scale < 0.59 || device_scale > 1.01) {
        return QX_ERR_INVALID_ARGUMENT;
    }
    return QX_OK;
}

/* ── Pool lifecycle ─────────────────────────────────────────── */

QXResult qx_pool_create(
    QXSize        soft_limit_bytes,
    QXSize        hard_limit_bytes,
    double        device_scale,
    QXMemlocPool* out_pool)
{
    uint32_t i;
    QXResult r;
    QXSize   page_size;
    QXSize   scaled_soft;
    QXSize   scaled_hard;
    QXSize   total_reserve;
    void*    base;
    uint8_t* cursor;

    if (out_pool == NULL) {
        return QX_ERR_NULL_HANDLE;
    }

    r = qx_pool_validate_pool_limits(soft_limit_bytes, hard_limit_bytes);
    if (r != QX_OK) {
        return r;
    }

    r = qx_pool_validate_device_scale(device_scale);
    if (r != QX_OK) {
        return r;
    }

    memset(out_pool, 0, sizeof(QXMemlocPool));

    scaled_soft = (QXSize)((double)soft_limit_bytes * device_scale);
    scaled_hard = (QXSize)((double)hard_limit_bytes * device_scale);

    if (scaled_soft < QX_POOL_MIN_BYTES) {
        scaled_soft = QX_POOL_MIN_BYTES;
    }
    if (scaled_hard < scaled_soft) {
        scaled_hard = (QXSize)((double)scaled_soft * QX_HARD_TO_SOFT_RATIO);
    }

    page_size = qx_pool_platform_page_size();
    if (page_size == 0u) {
        return QX_ERR_INTERNAL;
    }

    total_reserve = 0u;
    for (i = 0u; i < QX_POOL_SEGMENT_COUNT; ++i) {
        QXSize segment_hard = (QXSize)(
            (double)scaled_hard * qx_pool_segment_weights[i]
        );
        total_reserve += qx_pool_align_to_page(segment_hard, page_size);
    }

    base = mmap(
        NULL,
        (size_t)total_reserve,
        PROT_NONE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );
    if (base == MAP_FAILED) {
        return QX_ERR_INTERNAL;
    }

    out_pool->base                    = (uint8_t*)base;
    out_pool->total_reserved_bytes    = total_reserve;
    out_pool->total_committed_bytes   = 0u;
    out_pool->total_soft_limit_bytes  = scaled_soft;
    out_pool->total_hard_limit_bytes  = scaled_hard;
    out_pool->page_size               = page_size;
    out_pool->is_initialised          = QX_TRUE;

    cursor = out_pool->base;
    for (i = 0u; i < QX_POOL_SEGMENT_COUNT; ++i) {
        QXPoolSegmentRegion* region = &out_pool->segments[i];
        QXSize segment_soft;
        QXSize segment_hard;

        strncpy(
            region->segment_id,
            qx_pool_segment_ids[i],
            sizeof(region->segment_id) - 1u
        );
        region->segment_id[sizeof(region->segment_id) - 1u] = '\0';

        segment_soft = (QXSize)(
            (double)scaled_soft * qx_pool_segment_weights[i]
        );
        segment_hard = (QXSize)(
            (double)scaled_hard * qx_pool_segment_weights[i]
        );

        segment_soft = qx_pool_align_to_page(segment_soft, page_size);
        segment_hard = qx_pool_align_to_page(segment_hard, page_size);

        region->base             = cursor;
        region->reserved_bytes   = segment_hard;
        region->committed_bytes  = 0u;
        region->peak_committed   = 0u;
        region->soft_limit_bytes = segment_soft;
        region->hard_limit_bytes = segment_hard;

        cursor += segment_hard;
    }

    r = qx_pool_init_segment_mutexes();
    if (r != QX_OK) {
        munmap(base, (size_t)total_reserve);
        memset(out_pool, 0, sizeof(QXMemlocPool));
        return r;
    }

    return QX_OK;
}

void qx_pool_destroy(QXMemlocPool* pool)
{
    if (pool == NULL || !pool->is_initialised) {
        return;
    }

    if (pool->base != NULL) {
        munmap(pool->base, (size_t)pool->total_reserved_bytes);
        pool->base = NULL;
    }

    qx_pool_destroy_segment_mutexes();
    memset(pool, 0, sizeof(QXMemlocPool));
}
