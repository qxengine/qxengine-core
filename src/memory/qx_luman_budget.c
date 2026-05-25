/**
 * @file qx_luman_budget.c
 * @brief LUMAN — Constitutional Budget Calculator
 *
 * ALAMTOLOGI — QURANIC SCIENCE
 * Founder: Masa Bayu
 *
 * This file translates LUMAN pattern mathematics into
 * constitutional memory budgets for every segment.
 *
 * Repository: https://github.com/qxengine/qxengine-core
 * Date:       2026-05-25
 * Version:    1.0.0-rc.2
 */

#include "qxengine/memory/qx_luman.h"

#include <string.h>

#define QX_LUMAN_X_AV           50.0
#define QX_LUMAN_X_IMG        2000.0
#define QX_LUMAN_X_NET         500.0
#define QX_LUMAN_X_TOLERANCE    15.0

typedef struct {
    const char* segment_id;
    uint8_t     pola_base;
    double      declared_x;
    double      tolerance_pct;
} QXLumanSegmentDef;

static const QXLumanSegmentDef QX_LUMAN_SEGMENT_DEFS[] = {
    { "AV",   QX_LUMAN_POLA_6, QX_LUMAN_X_AV,  QX_LUMAN_X_TOLERANCE },
    { "IMG",  QX_LUMAN_POLA_4, QX_LUMAN_X_IMG, QX_LUMAN_X_TOLERANCE },
    { "NET",  QX_LUMAN_POLA_4, QX_LUMAN_X_NET, QX_LUMAN_X_TOLERANCE },
    { "CORE", QX_LUMAN_POLA_4, 5000.0,         QX_LUMAN_X_TOLERANCE },
    { "UI",   QX_LUMAN_POLA_4, 1000.0,         QX_LUMAN_X_TOLERANCE },
    { "DATA", QX_LUMAN_POLA_4, 3000.0,         QX_LUMAN_X_TOLERANCE },
    { "AI",   QX_LUMAN_POLA_6, 10000.0,        QX_LUMAN_X_TOLERANCE },
    { "SEC",  QX_LUMAN_POLA_4, 60000.0,        QX_LUMAN_X_TOLERANCE },
    { "LOG",  QX_LUMAN_POLA_4, 1000.0,         QX_LUMAN_X_TOLERANCE },
    { "TEMP", QX_LUMAN_POLA_4, 100.0,          QX_LUMAN_X_TOLERANCE }
};

#define QX_LUMAN_SEGMENT_DEF_COUNT \
    (sizeof(QX_LUMAN_SEGMENT_DEFS) / sizeof(QX_LUMAN_SEGMENT_DEFS[0]))

static const QXLumanSegmentDef* find_segment_def(const char* segment_id)
{
    size_t i;

    if (segment_id == NULL) {
        return NULL;
    }

    for (i = 0u; i < QX_LUMAN_SEGMENT_DEF_COUNT; ++i) {
        if (strcmp(QX_LUMAN_SEGMENT_DEFS[i].segment_id, segment_id) == 0) {
            return &QX_LUMAN_SEGMENT_DEFS[i];
        }
    }

    return NULL;
}

QXResult qx_luman_compute_budget(
    uint8_t         peringkat,
    uint8_t         pola_base,
    const char*     segment_id,
    double          declared_x,
    QXLumanBudget*  out_budget)
{
    QXLumanPattern pattern;
    uint8_t i;
    QXResult r;

    if (out_budget == NULL || segment_id == NULL) {
        return QX_ERR_INVALID_ARGUMENT;
    }
    if (peringkat > QX_LUMAN_PERINGKAT_MAX) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    memset(out_budget, 0, sizeof(*out_budget));
    memset(&pattern, 0, sizeof(pattern));

    r = qx_luman_compute_pattern(peringkat, pola_base, &pattern);
    if (r != QX_OK) {
        return r;
    }

    out_budget->peringkat = peringkat;
    out_budget->pola_base = pola_base;
    out_budget->slot_count = pattern.total_columns;
    out_budget->declared_x = declared_x;
    out_budget->tolerance_pct = QX_LUMAN_X_TOLERANCE;
    out_budget->core_bytes =
        pattern.core_value * QX_LUMAN_BASE_UNIT_BYTES;
    out_budget->soft_bytes =
        pattern.total_boxes * QX_LUMAN_BASE_UNIT_BYTES;
    out_budget->hard_bytes =
        (QXSize)((double)out_budget->soft_bytes * QX_LUMAN_HARD_RATIO);

    for (i = 0u; i < pattern.total_columns; ++i) {
        out_budget->slot_budgets[i] =
            pattern.columns[i] * QX_LUMAN_BASE_UNIT_BYTES;
    }

    strncpy(out_budget->segment_id, segment_id,
            sizeof(out_budget->segment_id) - 1u);

    return QX_OK;
}

QXResult qx_luman_budget_for_segment(
    uint8_t         peringkat,
    const char*     segment_id,
    QXLumanBudget*  out_budget)
{
    const QXLumanSegmentDef* def;

    if (segment_id == NULL || out_budget == NULL) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    def = find_segment_def(segment_id);
    if (def == NULL) {
        return QX_ERR_LEAF_NOT_FOUND;
    }

    return qx_luman_compute_budget(
        peringkat,
        def->pola_base,
        segment_id,
        def->declared_x,
        out_budget
    );
}

QXResult qx_luman_budget_all_segments(
    uint8_t         peringkat,
    QXLumanBudget*  out_budgets,
    uint8_t*        out_count)
{
    size_t i;

    if (out_budgets == NULL || out_count == NULL) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    *out_count = 0u;

    for (i = 0u; i < QX_LUMAN_SEGMENT_DEF_COUNT; ++i) {
        QXResult r = qx_luman_budget_for_segment(
            peringkat,
            QX_LUMAN_SEGMENT_DEFS[i].segment_id,
            &out_budgets[i]
        );
        if (r != QX_OK) {
            return r;
        }
        (*out_count)++;
    }

    return QX_OK;
}
