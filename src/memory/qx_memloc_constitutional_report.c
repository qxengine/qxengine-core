/* ============================================================
 * qx_memloc_constitutional_report.c
 * QXMemloc — Constitutional Reports and Queries
 *
 * User-facing report, OS signal forwarding, and query helpers
 * for the constitutional memory authority.
 *
 * ALAMTOLOGI — Quranic Science
 * Founded by Masa Bayu
 *
 * Owner : Masa Bayu
 * Date  : 2026-05-25
 * Repo  : https://github.com/qxengine/qxengine-core
 * ============================================================ */

#include "qxengine/memory/qx_memloc_constitutional.h"

#include <string.h>

static double clamp_score(double score)
{
    if (score < 0.0) {
        return 0.0;
    }
    if (score > 100.0) {
        return 100.0;
    }
    return score;
}

static QXCertificationTier certify_score(double score)
{
    if (score >= QX_SCORE_SOVEREIGN) {
        return QX_CERTIFICATION_SOVEREIGN;
    }
    if (score >= QX_SCORE_PROFESSIONAL) {
        return QX_CERTIFICATION_PROFESSIONAL;
    }
    if (score >= QX_SCORE_STANDARD) {
        return QX_CERTIFICATION_STANDARD;
    }
    return QX_CERTIFICATION_NONE;
}

QXResult qx_memloc_constitutional_report(
    QXMemlocConstitutional*  memloc,
    QXTimestamp              now_ms,
    QXConstitutionalReport*  out_report)
{
    QXEngineABAMeasurement measurement;
    QXEnforcementResult enforcement;
    double utilisation;
    double knowledge_score;
    double ethics_score;
    double economy_score;
    uint32_t i;
    QXResult r;

    if (out_report == NULL) {
        return QX_ERR_NULL_HANDLE;
    }

    memset(out_report, 0, sizeof(QXConstitutionalReport));
    memset(&measurement, 0, sizeof(measurement));
    memset(&enforcement, 0, sizeof(enforcement));

    r = qx_memloc_constitutional_cycle(
        memloc,
        now_ms,
        &measurement,
        &enforcement
    );
    if (r != QX_OK) {
        return r;
    }

    utilisation = qx_pool_total_utilisation_pct(&memloc->pool);
    knowledge_score = measurement.is_constant
        ? 100.0
        : clamp_score(100.0 - (measurement.drift_ratio * 100.0));
    ethics_score = (measurement.orphan_leaves == 0u) ? 100.0 : 50.0;
    economy_score = clamp_score(100.0 - utilisation);

    out_report->cycle_number = memloc->cycle_count;
    out_report->cycle_timestamp_ms = now_ms;
    out_report->aba_measurement = measurement;
    out_report->pool_utilisation_pct = utilisation;
    out_report->active_leaf_count = measurement.total_leaves_measured;
    out_report->orphan_leaf_count = measurement.orphan_leaves;

    out_report->law_report.law_scores[0].raw_score = 100.0;
    out_report->law_report.law_scores[1].raw_score = economy_score;
    out_report->law_report.law_scores[2].raw_score = ethics_score;
    out_report->law_report.law_scores[3].raw_score =
        enforcement.equilibrium_restored ? 100.0 : 70.0;
    out_report->law_report.law_scores[4].raw_score = knowledge_score;
    out_report->law_report.law_scores[5].raw_score = ethics_score;
    out_report->law_report.law_scores[6].raw_score = 100.0;
    out_report->law_report.law_scores[7].raw_score = economy_score;

    for (i = 0u; i < QX_LAW_COUNT; ++i) {
        QXLawScore* score = &out_report->law_report.law_scores[i];
        score->law_id = (QXLawId)(i + 1u);
        score->weighted_score = score->raw_score * QX_LAW_WEIGHTS[i];
        score->worst_severity = QX_SEVERITY_INFO;
        score->is_compliant = QX_TRUE;
    }

    out_report->universe_law_score =
        out_report->law_report.law_scores[0].weighted_score +
        out_report->law_report.law_scores[1].weighted_score +
        out_report->law_report.law_scores[2].weighted_score +
        out_report->law_report.law_scores[3].weighted_score;
    out_report->humanity_law_score =
        out_report->law_report.law_scores[4].weighted_score +
        out_report->law_report.law_scores[5].weighted_score +
        out_report->law_report.law_scores[6].weighted_score +
        out_report->law_report.law_scores[7].weighted_score;
    out_report->health_score =
        out_report->universe_law_score + out_report->humanity_law_score;
    out_report->certification = certify_score(out_report->health_score);
    out_report->law_report.health_score = out_report->health_score;
    out_report->law_report.certification =
        (QXCertificationId)out_report->certification;
    out_report->law_report.is_fully_compliant =
        (out_report->health_score >= QX_SCORE_SOVEREIGN) ? QX_TRUE : QX_FALSE;
    out_report->law_report.evaluated_at_ms = now_ms;

    memloc->health_score = out_report->health_score;
    memloc->certification = out_report->certification;

    return QX_OK;
}

QXResult qx_memloc_constitutional_notify_pressure(
    QXMemlocConstitutional* memloc,
    uint8_t                 pressure_tier)
{
    QXEnforcementResult result;

    if (memloc == NULL) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    memset(&result, 0, sizeof(result));
    return qx_enforce_pressure(memloc->enforce, pressure_tier, &result);
}

QXResult qx_memloc_constitutional_notify_lifecycle(
    QXMemlocConstitutional* memloc,
    uint8_t                 lifecycle_state)
{
    if (memloc == NULL) {
        return QX_ERR_INVALID_ARGUMENT;
    }
    return qx_flow_notify_lifecycle_change(memloc->flow, lifecycle_state);
}

QXResult qx_memloc_constitutional_notify_screen(
    QXMemlocConstitutional* memloc,
    const char*             screen_profile_id)
{
    if (memloc == NULL || screen_profile_id == NULL) {
        return QX_ERR_INVALID_ARGUMENT;
    }
    return qx_flow_notify_screen_profile_change(
        memloc->flow,
        screen_profile_id
    );
}

double qx_memloc_constitutional_health_score(
    const QXMemlocConstitutional* memloc)
{
    return (memloc != NULL) ? memloc->health_score : 0.0;
}

QXCertificationTier qx_memloc_constitutional_certification(
    const QXMemlocConstitutional* memloc)
{
    return (memloc != NULL)
        ? memloc->certification
        : QX_CERTIFICATION_NONE;
}

const char* qx_memloc_constitutional_instance_id(
    const QXMemlocConstitutional* memloc)
{
    return (memloc != NULL) ? memloc->instance_id : "";
}
