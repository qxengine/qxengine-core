/* ============================================================
 * qx_memloc_pool_luman.c
 * QXMemloc — Th [Tanah] — LUMAN Pool Lifecycle
 *
 * Creates pool regions from exact LUMAN segment budgets.
 *
 * ALAMTOLOGI — Quranic Science
 * Founded by Masa Bayu
 *
 * Owner : Masa Bayu
 * Date  : 2026-05-25
 * Repo  : https://github.com/qxengine/qxengine-core
 * ============================================================ */

#include "qx_memloc_pool_internal.h"

static const QXLumanBudget* find_luman_budget(
    const QXLumanInitResult* result,
    const char*              pool_segment_id)
{
    const char* short_id;
    uint8_t i;

    if (result == NULL || pool_segment_id == NULL) {
        return NULL;
    }

    short_id = (strncmp(pool_segment_id, "QLM-", 4u) == 0)
        ? pool_segment_id + 4u
        : pool_segment_id;

    for (i = 0u; i < result->segment_count; ++i) {
        if (strncmp(result->segment_budgets[i].segment_id,
                    short_id,
                    sizeof(result->segment_budgets[i].segment_id)) == 0) {
            return &result->segment_budgets[i];
        }
    }

    return NULL;
}

QXResult qx_pool_create_luman(
    const QXLumanInitResult* luman_result,
    QXMemlocPool*            out_pool)
{
    uint32_t i;
    QXResult r;
    QXSize page_size;
    QXSize total_soft;
    QXSize total_hard;
    void* base;
    uint8_t* cursor;

    if (luman_result == NULL || out_pool == NULL) {
        return QX_ERR_INVALID_ARGUMENT;
    }
    if (luman_result->initialized != QX_TRUE) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    page_size = qx_pool_platform_page_size();
    if (page_size == 0u) {
        return QX_ERR_INTERNAL;
    }

    total_soft = 0u;
    total_hard = 0u;
    for (i = 0u; i < QX_POOL_SEGMENT_COUNT; ++i) {
        const QXLumanBudget* budget =
            find_luman_budget(luman_result, qx_pool_segment_ids[i]);
        if (budget == NULL) {
            return QX_ERR_INVALID_ARGUMENT;
        }
        total_soft += qx_pool_align_to_page(budget->soft_bytes, page_size);
        total_hard += qx_pool_align_to_page(budget->hard_bytes, page_size);
    }

    base = mmap(
        NULL,
        (size_t)total_hard,
        PROT_NONE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );
    if (base == MAP_FAILED) {
        return QX_ERR_INTERNAL;
    }

    memset(out_pool, 0, sizeof(QXMemlocPool));
    out_pool->base = (uint8_t*)base;
    out_pool->total_reserved_bytes = total_hard;
    out_pool->total_committed_bytes = 0u;
    out_pool->total_soft_limit_bytes = total_soft;
    out_pool->total_hard_limit_bytes = total_hard;
    out_pool->page_size = page_size;
    out_pool->is_initialised = QX_TRUE;

    cursor = out_pool->base;
    for (i = 0u; i < QX_POOL_SEGMENT_COUNT; ++i) {
        QXPoolSegmentRegion* region = &out_pool->segments[i];
        const QXLumanBudget* budget =
            find_luman_budget(luman_result, qx_pool_segment_ids[i]);
        QXSize segment_soft = qx_pool_align_to_page(
            budget->soft_bytes, page_size);
        QXSize segment_hard = qx_pool_align_to_page(
            budget->hard_bytes, page_size);

        strncpy(region->segment_id,
                qx_pool_segment_ids[i],
                sizeof(region->segment_id) - 1u);
        region->segment_id[sizeof(region->segment_id) - 1u] = '\0';
        region->base = cursor;
        region->reserved_bytes = segment_hard;
        region->committed_bytes = 0u;
        region->peak_committed = 0u;
        region->soft_limit_bytes = segment_soft;
        region->hard_limit_bytes = segment_hard;

        cursor += segment_hard;
    }

    r = qx_pool_init_segment_mutexes();
    if (r != QX_OK) {
        munmap(base, (size_t)total_hard);
        memset(out_pool, 0, sizeof(QXMemlocPool));
        return r;
    }

    return QX_OK;
}
