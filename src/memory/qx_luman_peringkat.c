/**
 * @file qx_luman_peringkat.c
 * @brief LUMAN — Peringkat Detection and Pattern Computation
 *
 * ALAMTOLOGI — QURANIC SCIENCE
 * Founder: Masa Bayu
 *
 * This file implements the mathematical core of LUMAN:
 * Peringkat detection, pattern computation, and device profiles.
 *
 * Repository: https://github.com/qxengine/qxengine-core
 * Date:       2026-05-25
 * Version:    1.0.0-rc.2
 */

#include "qxengine/memory/qx_luman.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    uint64_t min_ram_bytes;
    uint8_t  peringkat;
    char     name[8];
    char     description[32];
} QXLumanRAMThreshold;

static const QXLumanRAMThreshold QX_LUMAN_RAM_THRESHOLDS[] = {
    { (16ULL * 1024ULL * 1024ULL * 1024ULL) + 1ULL,
      QX_LUMAN_PERINGKAT_TU, "Tu", "Flagship (>16 GB)" },
    { 12ULL * 1024ULL * 1024ULL * 1024ULL,
      QX_LUMAN_PERINGKAT_NA, "Na", "Sovereign (12-16 GB)" },
    { 8ULL * 1024ULL * 1024ULL * 1024ULL,
      QX_LUMAN_PERINGKAT_MA, "Ma", "Advanced (8 GB)" },
    { 6ULL * 1024ULL * 1024ULL * 1024ULL,
      QX_LUMAN_PERINGKAT_PA, "Pa", "Professional (6 GB)" },
    { 3ULL * 1024ULL * 1024ULL * 1024ULL,
      QX_LUMAN_PERINGKAT_GA, "Ga", "Standard (3-4 GB)" },
    { 2ULL * 1024ULL * 1024ULL * 1024ULL,
      QX_LUMAN_PERINGKAT_DU, "Du", "Entry (2 GB)" },
    { 0ULL, QX_LUMAN_PERINGKAT_SA, "Sa", "Minimum (<=1 GB)" }
};

#define QX_LUMAN_THRESHOLD_COUNT \
    (sizeof(QX_LUMAN_RAM_THRESHOLDS) / sizeof(QX_LUMAN_RAM_THRESHOLDS[0]))

static uint64_t luman_power(uint64_t base, uint8_t exp)
{
    uint64_t result = 1u;
    uint8_t i;

    for (i = 0u; i < exp; ++i) {
        result *= base;
    }

    return result;
}

static uint8_t luman_abs_diff(uint8_t a, uint8_t b)
{
    return (a >= b) ? (uint8_t)(a - b) : (uint8_t)(b - a);
}

static QXBool luman_validate_pola(uint8_t pola_base)
{
    return (pola_base == QX_LUMAN_POLA_4 ||
            pola_base == QX_LUMAN_POLA_6)
        ? QX_TRUE
        : QX_FALSE;
}

QXResult qx_luman_detect_peringkat(
    uint64_t  ram_bytes,
    uint8_t*  out_peringkat)
{
    size_t i;

    if (out_peringkat == NULL) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    for (i = 0u; i < QX_LUMAN_THRESHOLD_COUNT; ++i) {
        if (ram_bytes >= QX_LUMAN_RAM_THRESHOLDS[i].min_ram_bytes) {
            *out_peringkat = QX_LUMAN_RAM_THRESHOLDS[i].peringkat;
            return QX_OK;
        }
    }

    *out_peringkat = QX_LUMAN_PERINGKAT_SA;
    return QX_OK;
}

QXResult qx_luman_compute_pattern(
    uint8_t          peringkat,
    uint8_t          pola_base,
    QXLumanPattern*  out_pattern)
{
    uint8_t b;
    uint8_t a;
    uint64_t total;
    uint8_t i;

    if (out_pattern == NULL) {
        return QX_ERR_INVALID_ARGUMENT;
    }
    if (peringkat > QX_LUMAN_PERINGKAT_MAX ||
        luman_validate_pola(pola_base) == QX_FALSE) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    memset(out_pattern, 0, sizeof(*out_pattern));

    b = peringkat;
    a = pola_base;
    total = 0u;

    out_pattern->peringkat = b;
    out_pattern->pola_base = a;
    out_pattern->total_columns = (uint8_t)(2u * b + 1u);

    for (i = 0u; i < out_pattern->total_columns; ++i) {
        const uint8_t distance = luman_abs_diff(i, b);
        const uint8_t exponent = (uint8_t)(b - distance);
        const uint64_t value = luman_power(a, exponent);
        out_pattern->columns[i] = value;
        total += value;
    }

    out_pattern->total_boxes = total;
    out_pattern->core_value = luman_power(a, b);
    snprintf(out_pattern->symbol, sizeof(out_pattern->symbol),
             "%u 0 %u", (unsigned)a, (unsigned)b);
    strncpy(out_pattern->level_name,
            qx_luman_peringkat_name(b),
            sizeof(out_pattern->level_name) - 1u);

    return QX_OK;
}

uint64_t qx_luman_total_boxes(uint8_t peringkat, uint8_t pola_base)
{
    QXLumanPattern pattern;

    if (peringkat > QX_LUMAN_PERINGKAT_MAX ||
        luman_validate_pola(pola_base) == QX_FALSE) {
        return 0u;
    }

    if (qx_luman_compute_pattern(peringkat, pola_base, &pattern) != QX_OK) {
        return 0u;
    }

    return pattern.total_boxes;
}

uint64_t qx_luman_core_value(uint8_t peringkat, uint8_t pola_base)
{
    if (peringkat > QX_LUMAN_PERINGKAT_MAX ||
        luman_validate_pola(pola_base) == QX_FALSE) {
        return 0u;
    }

    return luman_power(pola_base, peringkat);
}

QXResult qx_luman_build_device_profile(
    uint64_t              ram_bytes,
    QXLumanDeviceProfile* out_profile)
{
    QXResult r;
    uint64_t ram_gb;

    if (out_profile == NULL || ram_bytes == 0u) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    memset(out_profile, 0, sizeof(*out_profile));

    out_profile->physical_ram_bytes = ram_bytes;
    out_profile->auto_detected = QX_TRUE;

    r = qx_luman_detect_peringkat(ram_bytes, &out_profile->peringkat);
    if (r != QX_OK) {
        return r;
    }

    strncpy(out_profile->peringkat_name,
            qx_luman_peringkat_name(out_profile->peringkat),
            sizeof(out_profile->peringkat_name) - 1u);

    ram_gb = ram_bytes / (1024ULL * 1024ULL * 1024ULL);
    snprintf(out_profile->device_description,
             sizeof(out_profile->device_description),
             "Peringkat %s - %llu GB RAM - LUMAN Level %u",
             out_profile->peringkat_name,
             (unsigned long long)ram_gb,
             (unsigned)out_profile->peringkat);

    return QX_OK;
}
