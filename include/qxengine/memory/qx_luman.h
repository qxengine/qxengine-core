/**
 * @file qx_luman.h
 * @brief QXEngine — LUMAN Constitutional Budget System
 *
 * ALAMTOLOGI — QURANIC SCIENCE
 * Founder: Masa Bayu
 *
 * LUMAN (Pola Alamtologi) is the mathematical discipline
 * within ALAMTOLOGI that governs bounded, proportional
 * pattern-based computation.
 *
 * LUMAN replaces device_scale with a constitutionally
 * derived Peringkat (level) determined by the device's
 * physical RAM. Every memory budget ceiling is produced
 * by the LUMAN formula — not by engineering estimates.
 *
 * Repository: https://github.com/qxengine/qxengine-core
 * Date:       2026-05-25
 * Version:    1.0.0-rc.2
 */

#ifndef QX_LUMAN_H
#define QX_LUMAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "qxengine/core/qx_types.h"
#include "qxengine/core/qx_error.h"

#define QX_LUMAN_POLA_4          4u
#define QX_LUMAN_POLA_6          6u

#define QX_LUMAN_PERINGKAT_SA    0u
#define QX_LUMAN_PERINGKAT_DU    1u
#define QX_LUMAN_PERINGKAT_GA    2u
#define QX_LUMAN_PERINGKAT_PA    3u
#define QX_LUMAN_PERINGKAT_MA    4u
#define QX_LUMAN_PERINGKAT_NA    5u
#define QX_LUMAN_PERINGKAT_TU    6u
#define QX_LUMAN_PERINGKAT_MAX   6u

#define QX_LUMAN_BASE_UNIT_BYTES (1ULL * 1024ULL * 1024ULL)
#define QX_LUMAN_HARD_RATIO      1.5
#define QX_LUMAN_MAX_COLUMNS     13u

#define QX_LUMAN_SEGMENT_AV      QX_LUMAN_POLA_6
#define QX_LUMAN_SEGMENT_IMG     QX_LUMAN_POLA_4
#define QX_LUMAN_SEGMENT_NET     QX_LUMAN_POLA_4

static inline const char* qx_luman_peringkat_name(uint8_t peringkat) {
    static const char* names[] = {
        "Sa", "Du", "Ga", "Pa", "Ma", "Na", "Tu"
    };
    if (peringkat > QX_LUMAN_PERINGKAT_MAX) return "Unknown";
    return names[peringkat];
}

typedef struct {
    uint8_t  peringkat;
    uint8_t  pola_base;
    uint8_t  total_columns;
    uint64_t columns[QX_LUMAN_MAX_COLUMNS];
    uint64_t total_boxes;
    uint64_t core_value;
    char     symbol[16];
    char     level_name[8];
} QXLumanPattern;

typedef struct {
    uint8_t     peringkat;
    uint8_t     pola_base;
    QXSize      soft_bytes;
    QXSize      hard_bytes;
    QXSize      core_bytes;
    QXSize      slot_budgets[QX_LUMAN_MAX_COLUMNS];
    uint8_t     slot_count;
    double      declared_x;
    double      tolerance_pct;
    char        segment_id[16];
} QXLumanBudget;

typedef struct {
    uint64_t physical_ram_bytes;
    uint8_t  peringkat;
    char     peringkat_name[8];
    char     device_description[64];
    QXBool   auto_detected;
} QXLumanDeviceProfile;

QX_API QXResult qx_luman_compute_pattern(
    uint8_t          peringkat,
    uint8_t          pola_base,
    QXLumanPattern*  out_pattern
);

QX_API QXResult qx_luman_compute_budget(
    uint8_t         peringkat,
    uint8_t         pola_base,
    const char*     segment_id,
    double          declared_x,
    QXLumanBudget*  out_budget
);

QX_API QXResult qx_luman_detect_peringkat(
    uint64_t  ram_bytes,
    uint8_t*  out_peringkat
);

QX_API QXResult qx_luman_build_device_profile(
    uint64_t              ram_bytes,
    QXLumanDeviceProfile* out_profile
);

QX_API uint64_t qx_luman_total_boxes(
    uint8_t peringkat,
    uint8_t pola_base
);

QX_API uint64_t qx_luman_core_value(
    uint8_t peringkat,
    uint8_t pola_base
);

QX_API QXResult qx_luman_budget_for_segment(
    uint8_t         peringkat,
    const char*     segment_id,
    QXLumanBudget*  out_budget
);

QX_API QXResult qx_luman_budget_all_segments(
    uint8_t         peringkat,
    QXLumanBudget*  out_budgets,
    uint8_t*        out_count
);

#ifdef __cplusplus
}
#endif

#endif /* QX_LUMAN_H */
