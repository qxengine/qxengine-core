// =============================================================================
// QXEngine Core – src/law/qx_law_enforcer.cpp
// Owner: Masa Bayu
// Created: 2026-05-24
// Description: Implementation of QXLawEnforcer – evaluates all eight
//              constitutional laws on every cycle, computes a composite
//              health score, assigns a certification tier, and maintains
//              a bounded audit-trail ring buffer (max 1 000 reports).
//
// Evaluation order (fixed, non-negotiable):
//   1. Law 1 – Pattern      6. Law 6 – Ethics
//   2. Law 2 – Limit        7. Law 7 – Creativity
//   3. Law 3 – Pairs        8. Law 8 – Economy
//   4. Law 4 – Equilibrium
//   5. Law 5 – Knowledge
//
// Repository: https://github.com/qxengine/qxengine-core
// =============================================================================

#include "qxengine/law/qx_law_enforcer.h"
#include "qxengine/law/qx_law_report.h"
#include "qxengine/core/qx_constants.h"
#include "qxengine/core/qx_error.h"
#include "qxengine/core/qx_types.h"

#include <cstring>
#include <mutex>
#include <atomic>
#include <chrono>
#include <new>

namespace qx::internal {
    void law_enforcer_evaluate_all(
        const QXLawEvaluationInput *,
        const QXEnforcerConfig *,
        QXLawReport *
    ) noexcept;
    void law_enforcer_evaluate_single(
        QXLawId,
        const QXLawEvaluationInput *,
        const QXEnforcerConfig *,
        QXLawReport *
    ) noexcept;
}

extern "C" {
    QX_API QXResult qx_law_report_zero_init(QXLawReport*);
}

namespace {

static QXTimestamp now_ms() noexcept {
    using namespace std::chrono;
    return static_cast<QXTimestamp>(
        duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()
        ).count()
    );
}

static uint64_t now_us() noexcept {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<microseconds>(
            high_resolution_clock::now().time_since_epoch()
        ).count()
    );
}

} // anonymous namespace

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

static void enforcer_append(
    QXLawEnforcer_s   *enforcer,
    const QXLawReport &report
) noexcept {
    enforcer->history[enforcer->history_head] = report;
    enforcer->history_head =
        (enforcer->history_head + 1u) % QX_ENFORCER_HISTORY_CAP;
    if (enforcer->history_count < QX_ENFORCER_HISTORY_CAP) {
        ++enforcer->history_count;
    }
}

static void enforcer_update_stats(
    QXLawEnforcer_s   *enforcer,
    const QXLawReport &report,
    uint64_t           duration_us
) noexcept {
    ++enforcer->total_evaluations;
    enforcer->sum_health_score  += report.health_score;
    enforcer->sum_duration_us   += duration_us;
    enforcer->last_evaluated_ms  = report.evaluated_at_ms;

    if (enforcer->first_evaluated_ms == 0u) {
        enforcer->first_evaluated_ms = report.evaluated_at_ms;
        enforcer->peak_health_score   = report.health_score;
        enforcer->trough_health_score = report.health_score;
    }

    if (report.health_score > enforcer->peak_health_score) {
        enforcer->peak_health_score = report.health_score;
    }
    if (report.health_score < enforcer->trough_health_score) {
        enforcer->trough_health_score = report.health_score;
    }

    if (report.has_fatal_violation)    { ++enforcer->fatal_evaluations;    }
    if (report.has_critical_violation) { ++enforcer->critical_evaluations;  }
    if (report.is_fully_compliant)     {
        ++enforcer->fully_compliant_evaluations;
    }

    for (uint32_t i = 0u; i < report.violation_count; ++i) {
        const QXViolation &v = report.violations[i];
        const uint32_t    idx =
            static_cast<uint32_t>(v.law_id) - 1u;
        if (idx < QX_LAW_COUNT) {
            ++enforcer->law_violation_counts[idx];
            if (v.severity == QX_SEVERITY_FATAL) {
                ++enforcer->law_fatal_counts[idx];
            }
        }
    }
}

} // anonymous namespace

extern "C" {

QX_API QXResult qx_law_enforcer_create(
    const QXEnforcerConfig *config,
    QXLawEnforcerHandle    *out_enforcer
) {
    if (!out_enforcer) { return QX_ERR_INVALID_ARGUMENT; }

    auto *enf = new (std::nothrow) QXLawEnforcer_s{};
    if (!enf) { return QX_ERR_INTERNAL; }

    enf->history = new (std::nothrow)
        QXLawReport[QX_ENFORCER_HISTORY_CAP]{};
    if (!enf->history) {
        delete enf;
        return QX_ERR_INTERNAL;
    }

    if (config) {
        enf->config = *config;
    } else {
        std::memset(&enf->config, 0, sizeof(QXEnforcerConfig));
        enf->config.measure_duration = QX_TRUE;
    }

    enf->history_count              = 0u;
    enf->history_head               = 0u;
    enf->sequence.store(1u, std::memory_order_relaxed);
    enf->total_evaluations          = 0u;
    enf->fatal_evaluations          = 0u;
    enf->critical_evaluations       = 0u;
    enf->fully_compliant_evaluations = 0u;
    enf->peak_health_score          = 0.0;
    enf->trough_health_score        = 100.0;
    enf->sum_health_score           = 0.0;
    enf->first_evaluated_ms         = 0u;
    enf->last_evaluated_ms          = 0u;
    enf->sum_duration_us            = 0u;
    std::memset(enf->law_violation_counts, 0,
                sizeof(enf->law_violation_counts));
    std::memset(enf->law_fatal_counts, 0,
                sizeof(enf->law_fatal_counts));

    *out_enforcer = enf;
    return QX_OK;
}

QX_API void qx_law_enforcer_destroy(QXLawEnforcerHandle enforcer) {
    if (!enforcer) { return; }
    delete[] enforcer->history;
    delete enforcer;
}

QX_API QXResult qx_law_enforcer_evaluate(
    QXLawEnforcerHandle          enforcer,
    const QXLawEvaluationInput  *input,
    QXLawReport                 *out_report
) {
    if (!enforcer)  { return QX_ERR_NULL_HANDLE;      }
    if (!input)     { return QX_ERR_INVALID_ARGUMENT; }
    if (input->segment_count == 0u ||
        input->segment_count > QX_EVAL_MAX_SEGMENTS) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(enforcer->mtx);

    const uint64_t start_us =
        enforcer->config.measure_duration ? now_us() : 0u;

    QXLawReport report{};
    qx_law_report_zero_init(&report);
    report.sequence        =
        enforcer->sequence.fetch_add(1u, std::memory_order_relaxed);
    report.evaluated_at_ms = input->captured_at_ms
                           ? input->captured_at_ms : now_ms();

    qx::internal::law_enforcer_evaluate_all(
        input, &enforcer->config, &report);

    if (enforcer->config.measure_duration) {
        report.evaluation_duration_us = now_us() - start_us;
    }

    enforcer_append(enforcer, report);
    enforcer_update_stats(enforcer, report,
                          report.evaluation_duration_us);

    if (out_report) { *out_report = report; }

    if (enforcer->config.fail_fast_on_fatal &&
        report.has_fatal_violation)
    {
        for (uint32_t i = 0u; i < report.violation_count; ++i) {
            if (report.violations[i].severity == QX_SEVERITY_FATAL) {
                const QXLawId law = report.violations[i].law_id;
                switch (law) {
                    case QX_LAW_PATTERN:     return QX_ERR_LAW_PATTERN;
                    case QX_LAW_LIMIT:       return QX_ERR_LAW_LIMIT;
                    case QX_LAW_PAIRS:       return QX_ERR_LAW_PAIRS;
                    case QX_LAW_EQUILIBRIUM: return QX_ERR_LAW_EQUILIBRIUM;
                    case QX_LAW_KNOWLEDGE:   return QX_ERR_LAW_KNOWLEDGE;
                    case QX_LAW_ETHICS:      return QX_ERR_LAW_ETHICS;
                    case QX_LAW_CREATIVITY:  return QX_ERR_LAW_CREATIVITY;
                    case QX_LAW_ECONOMY:     return QX_ERR_LAW_ECONOMY;
                    default:                 return QX_ERR_INTERNAL;
                }
            }
        }
    }
    return QX_OK;
}

QX_API QXResult qx_law_enforcer_evaluate_single(
    QXLawEnforcerHandle          enforcer,
    QXLawId                      law_id,
    const QXLawEvaluationInput  *input,
    QXLawReport                 *out_report
) {
    if (!enforcer || !out_report) { return QX_ERR_NULL_HANDLE;      }
    if (!input)                   { return QX_ERR_INVALID_ARGUMENT; }
    if (law_id < 1 ||
        static_cast<uint32_t>(law_id) > QX_LAW_COUNT) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    QXLawReport report{};
    qx_law_report_zero_init(&report);
    report.evaluated_at_ms = now_ms();

    qx::internal::law_enforcer_evaluate_single(
        law_id, input, &enforcer->config, &report);

    *out_report = report;
    return QX_OK;
}

} // extern "C"
