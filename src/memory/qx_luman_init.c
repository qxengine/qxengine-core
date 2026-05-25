/**
 * @file qx_luman_init.c
 * @brief LUMAN — Constitutional Initialisation Sequence
 *
 * ALAMTOLOGI — QURANIC SCIENCE
 * Founder: Masa Bayu
 *
 * This file owns the LUMAN initialisation sequence:
 * RAM -> Peringkat -> budgets -> one init result.
 *
 * Repository: https://github.com/qxengine/qxengine-core
 * Date:       2026-05-25
 * Version:    1.0.0-rc.3
 */

#include "qxengine/memory/qx_luman_init.h"
#include "platform/qx_platform_ram.h"

#include <stdio.h>
#include <string.h>

static QXSize luman_init_total_soft_budget(
    const QXLumanBudget* budgets,
    uint8_t              count)
{
    QXSize total = 0u;
    uint8_t i;

    for (i = 0u; i < count; ++i) {
        total += budgets[i].soft_bytes;
    }

    return total;
}

static QXSize luman_init_total_hard_budget(
    const QXLumanBudget* budgets,
    uint8_t              count)
{
    QXSize total = 0u;
    uint8_t i;

    for (i = 0u; i < count; ++i) {
        total += budgets[i].hard_bytes;
    }

    return total;
}

static void luman_init_log_profile(
    const QXLumanInitResult* result,
    char*                    out_log,
    size_t                   log_size)
{
    if (result == NULL || out_log == NULL || log_size == 0u) {
        return;
    }

    snprintf(out_log, log_size,
        "[LUMAN] Constitutional initialisation complete\n"
        "[LUMAN] Device:     %s\n"
        "[LUMAN] Peringkat:  %s (level %u)\n"
        "[LUMAN] RAM:        %llu MB\n"
        "[LUMAN] Pool soft:  %llu MB\n"
        "[LUMAN] Pool hard:  %llu MB\n"
        "[LUMAN] Segments:   %u\n"
        "[LUMAN] AV budget:  %llu MB (Pola 6)\n"
        "[LUMAN] IMG budget: %llu MB (Pola 4)\n"
        "[LUMAN] NET budget: %llu MB (Pola 4 OAP)\n",
        result->device_profile.device_description,
        result->device_profile.peringkat_name,
        (unsigned)result->device_profile.peringkat,
        (unsigned long long)(
            result->device_profile.physical_ram_bytes / (1024ULL * 1024ULL)),
        (unsigned long long)(result->total_soft_bytes / (1024ULL * 1024ULL)),
        (unsigned long long)(result->total_hard_bytes / (1024ULL * 1024ULL)),
        (unsigned)result->segment_count,
        (unsigned long long)(result->av_budget.soft_bytes / (1024ULL * 1024ULL)),
        (unsigned long long)(result->img_budget.soft_bytes / (1024ULL * 1024ULL)),
        (unsigned long long)(result->net_budget.soft_bytes / (1024ULL * 1024ULL))
    );
}

static QXResult luman_init_from_ram(
    uint64_t           ram_bytes,
    QXLumanInitResult* out_result)
{
    QXResult r;
    uint8_t peringkat;

    if (out_result == NULL || ram_bytes == 0u) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    memset(out_result, 0, sizeof(*out_result));

    r = qx_luman_build_device_profile(ram_bytes, &out_result->device_profile);
    if (r != QX_OK) {
        return r;
    }

    peringkat = out_result->device_profile.peringkat;

    r = qx_luman_budget_all_segments(
        peringkat,
        out_result->segment_budgets,
        &out_result->segment_count
    );
    if (r != QX_OK) {
        return r;
    }

    r = qx_luman_budget_for_segment(peringkat, "AV", &out_result->av_budget);
    if (r != QX_OK) {
        return r;
    }

    r = qx_luman_budget_for_segment(peringkat, "IMG", &out_result->img_budget);
    if (r != QX_OK) {
        return r;
    }

    r = qx_luman_budget_for_segment(peringkat, "NET", &out_result->net_budget);
    if (r != QX_OK) {
        return r;
    }

    out_result->total_soft_bytes = luman_init_total_soft_budget(
        out_result->segment_budgets,
        out_result->segment_count
    );
    out_result->total_hard_bytes = luman_init_total_hard_budget(
        out_result->segment_budgets,
        out_result->segment_count
    );
    out_result->initialized = QX_TRUE;

    luman_init_log_profile(
        out_result,
        out_result->log_buffer,
        sizeof(out_result->log_buffer)
    );

    return QX_OK;
}

QXResult qx_luman_init(QXLumanInitResult* out_result)
{
    uint64_t ram_bytes;

    if (out_result == NULL) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    ram_bytes = qx_platform_ram_bytes();
    if (ram_bytes == 0u) {
        ram_bytes = 1ULL * 1024ULL * 1024ULL * 1024ULL;
    }

    return luman_init_from_ram(ram_bytes, out_result);
}

QXResult qx_luman_init_with_ram(
    uint64_t           ram_bytes,
    QXLumanInitResult* out_result)
{
    return luman_init_from_ram(ram_bytes, out_result);
}
