/**
 * @file qx_luman_init.h
 * @brief LUMAN — Constitutional Initialisation Header
 *
 * ALAMTOLOGI — QURANIC SCIENCE
 * Founder: Masa Bayu
 *
 * Declares the LUMAN initialisation result structure and the
 * initialisation functions that derive memory budgets from RAM.
 *
 * Repository: https://github.com/qxengine/qxengine-core
 * Date:       2026-05-25
 * Version:    1.0.0-rc.3
 */

#ifndef QX_LUMAN_INIT_H
#define QX_LUMAN_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "qxengine/memory/qx_luman.h"

#define QX_LUMAN_INIT_MAX_SEGMENTS 16u
#define QX_LUMAN_INIT_LOG_SIZE     512u

typedef struct {
    QXLumanDeviceProfile  device_profile;
    QXLumanBudget         segment_budgets[QX_LUMAN_INIT_MAX_SEGMENTS];
    uint8_t               segment_count;
    QXLumanBudget         av_budget;
    QXLumanBudget         img_budget;
    QXLumanBudget         net_budget;
    QXSize                total_soft_bytes;
    QXSize                total_hard_bytes;
    QXBool                initialized;
    char                  log_buffer[QX_LUMAN_INIT_LOG_SIZE];
} QXLumanInitResult;

QX_API QXResult qx_luman_init(QXLumanInitResult* out_result);

QX_API QXResult qx_luman_init_with_ram(
    uint64_t           ram_bytes,
    QXLumanInitResult* out_result
);

#ifdef __cplusplus
}
#endif

#endif /* QX_LUMAN_INIT_H */
