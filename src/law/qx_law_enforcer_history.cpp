// =============================================================================
// QXEngine Core – src/law/qx_law_enforcer_history.cpp
// Owner: Masa Bayu
// Created: 2026-05-24
// Description: Report history, statistics, and health queries for QXLawEnforcer.
//              Split from qx_law_enforcer.cpp to satisfy the 500-line limit.
//
// Repository: https://github.com/qxengine/qxengine-core
// =============================================================================

#include "qxengine/law/qx_law_enforcer.h"
#include "qxengine/law/qx_law_report.h"
#include "qxengine/core/qx_constants.h"
#include "qxengine/core/qx_error.h"
#include "qxengine/core/qx_types.h"

#include <mutex>
#include <atomic>
#include <algorithm>

struct QXLawEnforcer_s {
    QXEnforcerConfig    config;
    QXLawReport        *history;
    uint32_t            history_count;
    uint32_t            history_head;
    std::atomic<uint64_t> sequence;
    uint64_t    total_evaluations;
    uint64_t    fatal_evaluations;
    uint64_t    critical_evaluations;
    uint64_t    fully_compliant_evaluations;
    double      peak_health_score;
    double      trough_health_score;
    double      sum_health_score;
    uint64_t    law_violation_counts[QX_LAW_COUNT];
    uint64_t    law_fatal_counts[QX_LAW_COUNT];
    QXTimestamp first_evaluated_ms;
    QXTimestamp last_evaluated_ms;
    uint64_t    sum_duration_us;
    mutable std::mutex mtx;
};

namespace {

static uint32_t enforcer_physical_index(
    const QXLawEnforcer_s *enforcer,
    uint32_t               logical_index
) noexcept {
    const uint32_t cap    = QX_ENFORCER_HISTORY_CAP;
    const uint32_t cnt    = enforcer->history_count;
    const uint32_t head   = enforcer->history_head;
    const uint32_t oldest = (cnt < cap) ? 0u : head;
    return (oldest + logical_index) % cap;
}

} // anonymous namespace

extern "C" {

QX_API QXResult qx_law_enforcer_report_count(
    QXLawEnforcerHandle  enforcer,
    uint32_t            *out_count
) {
    if (!enforcer || !out_count) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(enforcer->mtx);
    *out_count = enforcer->history_count;
    return QX_OK;
}

QX_API QXResult qx_law_enforcer_report_at(
    QXLawEnforcerHandle  enforcer,
    uint32_t             index,
    QXLawReport         *out_report
) {
    if (!enforcer || !out_report) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(enforcer->mtx);
    if (index >= enforcer->history_count) {
        return QX_ERR_INVALID_ARGUMENT;
    }
    *out_report =
        enforcer->history[enforcer_physical_index(enforcer, index)];
    return QX_OK;
}

QX_API QXResult qx_law_enforcer_latest_report(
    QXLawEnforcerHandle  enforcer,
    QXLawReport         *out_report
) {
    if (!enforcer || !out_report) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(enforcer->mtx);
    if (enforcer->history_count == 0u) {
        return QX_ERR_INVALID_ARGUMENT;
    }
    *out_report = enforcer->history[
        enforcer_physical_index(enforcer,
                                enforcer->history_count - 1u)];
    return QX_OK;
}

QX_API QXResult qx_law_enforcer_reports_copy(
    QXLawEnforcerHandle  enforcer,
    QXLawReport         *out_reports,
    uint32_t             capacity,
    uint32_t            *out_written
) {
    if (!enforcer || !out_reports || !out_written) {
        return QX_ERR_NULL_HANDLE;
    }
    if (capacity == 0u) { return QX_ERR_INVALID_ARGUMENT; }
    std::lock_guard<std::mutex> lock(enforcer->mtx);

    const uint32_t to_copy =
        std::min(capacity, enforcer->history_count);
    for (uint32_t i = 0u; i < to_copy; ++i) {
        out_reports[i] =
            enforcer->history[enforcer_physical_index(enforcer, i)];
    }
    *out_written = to_copy;
    return QX_OK;
}

QX_API QXResult qx_law_enforcer_find_violation_report(
    QXLawEnforcerHandle  enforcer,
    QXLawId              law_id,
    QXLawReport         *out_report
) {
    if (!enforcer || !out_report) { return QX_ERR_NULL_HANDLE; }
    if (law_id < 1 ||
        static_cast<uint32_t>(law_id) > QX_LAW_COUNT) {
        return QX_ERR_INVALID_ARGUMENT;
    }
    std::lock_guard<std::mutex> lock(enforcer->mtx);
    if (enforcer->history_count == 0u) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    for (uint32_t i = enforcer->history_count; i-- > 0u; ) {
        const QXLawReport &r =
            enforcer->history[enforcer_physical_index(enforcer, i)];
        for (uint32_t v = 0u; v < r.violation_count; ++v) {
            if (r.violations[v].law_id == law_id) {
                *out_report = r;
                return QX_OK;
            }
        }
    }
    return QX_ERR_NOT_SUPPORTED;
}

QX_API QXResult qx_law_enforcer_clear_history(
    QXLawEnforcerHandle enforcer
) {
    if (!enforcer) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(enforcer->mtx);
    enforcer->history_count = 0u;
    enforcer->history_head  = 0u;
    return QX_OK;
}

QX_API QXResult qx_law_enforcer_stats(
    QXLawEnforcerHandle  enforcer,
    QXEnforcerStats     *out_stats
) {
    if (!enforcer || !out_stats) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(enforcer->mtx);

    out_stats->total_evaluations =
        enforcer->total_evaluations;
    out_stats->fatal_evaluations =
        enforcer->fatal_evaluations;
    out_stats->critical_evaluations =
        enforcer->critical_evaluations;
    out_stats->fully_compliant_evaluations =
        enforcer->fully_compliant_evaluations;
    out_stats->peak_health_score   = enforcer->peak_health_score;
    out_stats->trough_health_score = enforcer->trough_health_score;
    out_stats->mean_health_score   =
        (enforcer->total_evaluations > 0u)
        ? enforcer->sum_health_score /
          static_cast<double>(enforcer->total_evaluations)
        : 0.0;
    out_stats->history_count       = enforcer->history_count;
    out_stats->first_evaluated_ms  = enforcer->first_evaluated_ms;
    out_stats->last_evaluated_ms   = enforcer->last_evaluated_ms;
    out_stats->mean_evaluation_duration_us =
        (enforcer->total_evaluations > 0u)
        ? enforcer->sum_duration_us / enforcer->total_evaluations
        : 0u;

    for (uint32_t i = 0u; i < QX_LAW_COUNT; ++i) {
        out_stats->law_violation_counts[i] =
            enforcer->law_violation_counts[i];
        out_stats->law_fatal_counts[i] =
            enforcer->law_fatal_counts[i];
    }

    if (enforcer->history_count > 0u) {
        const QXLawReport &last =
            enforcer->history[
                enforcer_physical_index(enforcer,
                    enforcer->history_count - 1u)];
        out_stats->last_health_score  = last.health_score;
        out_stats->last_certification = last.certification;
    } else {
        out_stats->last_health_score  = 0.0;
        out_stats->last_certification = QX_CERT_INVALID;
    }
    return QX_OK;
}

QX_API QXResult qx_law_enforcer_health_score(
    QXLawEnforcerHandle  enforcer,
    double              *out_score
) {
    if (!enforcer || !out_score) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(enforcer->mtx);
    if (enforcer->history_count == 0u) {
        return QX_ERR_INVALID_ARGUMENT;
    }
    const QXLawReport &last =
        enforcer->history[
            enforcer_physical_index(enforcer,
                enforcer->history_count - 1u)];
    *out_score = last.health_score;
    return QX_OK;
}

QX_API QXResult qx_law_enforcer_certification(
    QXLawEnforcerHandle  enforcer,
    QXCertificationId   *out_certification
) {
    if (!enforcer || !out_certification) {
        return QX_ERR_NULL_HANDLE;
    }
    std::lock_guard<std::mutex> lock(enforcer->mtx);
    if (enforcer->history_count == 0u) {
        return QX_ERR_INVALID_ARGUMENT;
    }
    const QXLawReport &last =
        enforcer->history[
            enforcer_physical_index(enforcer,
                enforcer->history_count - 1u)];
    *out_certification = last.certification;
    return QX_OK;
}

QX_API QXResult qx_law_enforcer_is_compliant(
    QXLawEnforcerHandle  enforcer,
    QXBool              *out_compliant
) {
    if (!enforcer || !out_compliant) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(enforcer->mtx);
    if (enforcer->history_count == 0u) {
        return QX_ERR_INVALID_ARGUMENT;
    }
    const QXLawReport &last =
        enforcer->history[
            enforcer_physical_index(enforcer,
                enforcer->history_count - 1u)];
    *out_compliant = last.is_fully_compliant;
    return QX_OK;
}

} // extern "C"
