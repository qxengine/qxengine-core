/**
 * @file qx_platform_ram.h
 * @brief Platform RAM Detection — Interface
 *
 * ALAMTOLOGI — QURANIC SCIENCE
 * Founder: Masa Bayu
 *
 * Declares the platform RAM detection function used by LUMAN.
 *
 * Repository: https://github.com/qxengine/qxengine-core
 * Date:       2026-05-25
 * Version:    1.0.0-rc.3
 */

#ifndef QX_PLATFORM_RAM_H
#define QX_PLATFORM_RAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

uint64_t qx_platform_ram_bytes(void);

#ifdef __cplusplus
}
#endif

#endif /* QX_PLATFORM_RAM_H */
