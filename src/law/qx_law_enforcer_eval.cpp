// =============================================================================
// QXEngine Core – src/law/qx_law_enforcer_eval.cpp
// Owner: Masa Bayu
// Created: 2026-05-24
// Description: Per-law evaluators and score aggregation for QXLawEnforcer.
//              Split from qx_law_enforcer.cpp to satisfy the 500-line limit.
//
// Repository: https://github.com/qxengine/qxengine-core
// =============================================================================

#include "qxengine/law/qx_law_report.h"
#include "qxengine/law/qx_law_enforcer_types.h"
#include "qxengine/core/qx_constants.h"
#include "qxengine/core/qx_error.h"
#include "qxengine/core/qx_types.h"

#include <cstring>
#include <cstdio>
#include <chrono>

namespace qx::internal {
    QXCertificationId certification_from_score(double) noexcept;
}

extern "C" {
    QX_API QXResult qx_violation_format_code(QXLawId, QXSeverityId, char*);
}

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

namespace {

static QXTimestamp now_ms() noexcept {
    using namespace std::chrono;
    return static_cast<QXTimestamp>(
        duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()
        ).count()
    );
}

static void push_violation(
    QXLawReport   *report,
    QXLawId        law_id,
    QXSeverityId   severity,
    const char    *message,
    const char    *detail,
    bool           is_recurring
) noexcept {
    if (report->violation_count >= QX_REPORT_MAX_VIOLATIONS) { return; }

    QXViolation &v = report->violations[report->violation_count++];
    v.law_id       = law_id;
    v.severity     = severity;
    v.detected_at_ms = now_ms();
    v.is_recurring   = is_recurring ? QX_TRUE : QX_FALSE;

    qx_violation_format_code(law_id, severity, v.error_code);

    if (message) {
        std::strncpy(v.message, message, QX_VIOLATION_MESSAGE_MAX - 1u);
        v.message[QX_VIOLATION_MESSAGE_MAX - 1u] = '\0';
    }
    if (detail) {
        std::strncpy(v.detail, detail, QX_VIOLATION_DETAIL_MAX - 1u);
        v.detail[QX_VIOLATION_DETAIL_MAX - 1u] = '\0';
    }

    const uint32_t idx = static_cast<uint32_t>(law_id) - 1u;
    if (severity > report->law_scores[idx].worst_severity) {
        report->law_scores[idx].worst_severity = severity;
    }
    report->law_scores[idx].violation_count++;
    report->law_scores[idx].is_compliant = QX_FALSE;
}

static void evaluate_law1(
    const QXLawEvaluationInput *input,
    QXLawReport                *report
) noexcept {
    if (input->unlabelled_allocation_count > 0u) {
        char detail[QX_VIOLATION_DETAIL_MAX];
        std::snprintf(detail, sizeof(detail),
            "%u unlabelled allocation(s) detected",
            input->unlabelled_allocation_count);
        push_violation(report,
            QX_LAW_PATTERN, QX_SEVERITY_FATAL,
            "Allocation without valid label violates Law 1 (Pattern)",
            detail, false);
        report->law_scores[0].raw_score = 0.0;
    }
}

static void evaluate_law2(
    const QXLawEvaluationInput *input,
    QXLawReport                *report
) noexcept {
    for (uint32_t i = 0u; i < input->segment_count; ++i) {
        const QXLawSegmentInput &seg = input->segments[i];

        if (seg.hard_utilisation > 1.0) {
            char msg[QX_VIOLATION_MESSAGE_MAX];
            std::snprintf(msg, sizeof(msg),
                "Segment %s hard budget exceeded (%.1f%%)",
                seg.segment_id,
                seg.hard_utilisation * 100.0);
            push_violation(report,
                QX_LAW_LIMIT, QX_SEVERITY_FATAL,
                msg, seg.segment_id, false);
            report->law_scores[1].raw_score = 0.0;

        } else if (seg.soft_utilisation > 1.0) {
            char msg[QX_VIOLATION_MESSAGE_MAX];
            std::snprintf(msg, sizeof(msg),
                "Segment %s soft budget exceeded (%.1f%%)",
                seg.segment_id,
                seg.soft_utilisation * 100.0);
            push_violation(report,
                QX_LAW_LIMIT, QX_SEVERITY_WARNING,
                msg, seg.segment_id, false);
            if (report->law_scores[1].raw_score > 75.0) {
                report->law_scores[1].raw_score = 75.0;
            }
        }
    }
}

static void evaluate_law3(
    const QXLawEvaluationInput *input,
    QXLawReport                *report
) noexcept {
    for (uint32_t i = 0u; i < input->segment_count; ++i) {
        const QXLawSegmentInput &seg = input->segments[i];
        if (seg.total_allocations == 0u) { continue; }

        if (seg.pairs_ratio < QX_MIN_PAIRS_RATIO) {
            char msg[QX_VIOLATION_MESSAGE_MAX];
            std::snprintf(msg, sizeof(msg),
                "Segment %s pairs ratio %.2f below minimum %.2f",
                seg.segment_id,
                seg.pairs_ratio,
                QX_MIN_PAIRS_RATIO);

            const QXSeverityId sev =
                (seg.pairs_ratio < 0.25) ? QX_SEVERITY_CRITICAL
                                         : QX_SEVERITY_WARNING;
            push_violation(report,
                QX_LAW_PAIRS, sev, msg, seg.segment_id, false);

            const double degrade = seg.pairs_ratio / QX_MIN_PAIRS_RATIO;
            const double new_score = degrade * 100.0;
            if (new_score < report->law_scores[2].raw_score) {
                report->law_scores[2].raw_score = new_score;
            }
        }
    }
}

static void evaluate_law4(
    const QXLawEvaluationInput *input,
    QXLawReport                *report
) noexcept {
    uint32_t overloaded = 0u;
    uint32_t starved    = 0u;

    for (uint32_t i = 0u; i < input->segment_count; ++i) {
        const QXLawSegmentInput &seg = input->segments[i];
        const double pct = seg.hard_utilisation * 100.0;

        if (pct >= QX_EQUILIBRIUM_OVERLOAD_PCT)  { ++overloaded; }
        if (pct <  QX_EQUILIBRIUM_STARVE_PCT
            && seg.used_bytes > 0u)               { ++starved;    }
    }

    if (overloaded > 0u && starved > 0u) {
        char msg[QX_VIOLATION_MESSAGE_MAX];
        std::snprintf(msg, sizeof(msg),
            "Equilibrium broken: %u overloaded, %u starved segment(s)",
            overloaded, starved);
        push_violation(report,
            QX_LAW_EQUILIBRIUM, QX_SEVERITY_CRITICAL,
            msg, nullptr, false);
        report->law_scores[3].raw_score = 50.0;
    }
}

static void evaluate_law5(
    const QXLawEvaluationInput *input,
    const QXEnforcerConfig     *cfg,
    QXLawReport                *report
) noexcept {
    const double threshold = (cfg && cfg->min_knowledge_score > 0.0)
                            ? cfg->min_knowledge_score
                            : QX_MIN_KNOWLEDGE_SCORE;
    const double score     = input->knowledge_score;

    QXSeverityId sev = QX_SEVERITY_INFO;
    if      (score < 50.0)      { sev = QX_SEVERITY_FATAL;    }
    else if (score < 70.0)      { sev = QX_SEVERITY_CRITICAL; }
    else if (score < threshold) { sev = QX_SEVERITY_WARNING;  }

    if (sev > QX_SEVERITY_INFO) {
        char msg[QX_VIOLATION_MESSAGE_MAX];
        std::snprintf(msg, sizeof(msg),
            "Knowledge score %.1f below threshold %.1f",
            score, threshold);
        push_violation(report,
            QX_LAW_KNOWLEDGE, sev, msg, nullptr, false);
        report->law_scores[4].raw_score = score;
    }
}

static void evaluate_law6(
    const QXLawEvaluationInput *input,
    QXLawReport                *report
) noexcept {
    const struct { QXBool value; const char *name; } flags[5] = {
        { input->dark_patterns_prohibited,   "darkPatternsProhibited"  },
        { input->deceptive_flows_prohibited, "deceptiveFlowsProhibited"},
        { input->manipulative_ux_prohibited, "manipulativeUXProhibited"},
        { input->privacy_first_design,       "privacyFirstDesign"      },
        { input->transparent_data_usage,     "transparentDataUsage"    }
    };

    for (uint32_t i = 0u; i < 5u; ++i) {
        if (flags[i].value == QX_FALSE) {
            char msg[QX_VIOLATION_MESSAGE_MAX];
            std::snprintf(msg, sizeof(msg),
                "Ethics flag '%s' is inactive", flags[i].name);
            push_violation(report,
                QX_LAW_ETHICS, QX_SEVERITY_FATAL,
                msg, flags[i].name, false);
            report->law_scores[5].raw_score = 0.0;
        }
    }
}

static void evaluate_law7(
    const QXLawEvaluationInput *input,
    const QXEnforcerConfig     *cfg,
    QXLawReport                *report
) noexcept {
    if (input->native_first_policy == QX_FALSE) {
        push_violation(report,
            QX_LAW_CREATIVITY, QX_SEVERITY_FATAL,
            "nativeFirstPolicy is inactive (Law 7)",
            nullptr, false);
        report->law_scores[6].raw_score = 0.0;
        return;
    }

    const double min_util = (cfg && cfg->min_native_utilisation > 0.0)
                           ? cfg->min_native_utilisation
                           : QX_MIN_NATIVE_UTILISATION;
    const double ratio = input->native_utilisation_ratio;

    if (ratio < min_util) {
        const QXSeverityId sev =
            (ratio < 0.25) ? QX_SEVERITY_CRITICAL : QX_SEVERITY_WARNING;
        char msg[QX_VIOLATION_MESSAGE_MAX];
        std::snprintf(msg, sizeof(msg),
            "Native utilisation %.2f below minimum %.2f",
            ratio, min_util);
        push_violation(report,
            QX_LAW_CREATIVITY, sev, msg, nullptr, false);
        report->law_scores[6].raw_score = (ratio / min_util) * 100.0;
    }
}

static void evaluate_law8(
    const QXLawEvaluationInput *input,
    const QXEnforcerConfig     *cfg,
    QXLawReport                *report
) noexcept {
    const double max_battery =
        (cfg && cfg->max_battery_drain_pct > 0.0)
        ? cfg->max_battery_drain_pct : QX_MAX_BATTERY_DRAIN_PCT;
    const double max_network =
        (cfg && cfg->max_network_redundancy_pct > 0.0)
        ? cfg->max_network_redundancy_pct : QX_MAX_NETWORK_REDUNDANCY;

    const double battery = input->battery_drain_pct_per_10min;
    if (battery > max_battery * 1.5) {
        char msg[QX_VIOLATION_MESSAGE_MAX];
        std::snprintf(msg, sizeof(msg),
            "Battery drain %.2f%% exceeds 150%% of limit %.2f%%",
            battery, max_battery);
        push_violation(report, QX_LAW_ECONOMY, QX_SEVERITY_FATAL,
            msg, "batteryDrain", false);
        report->law_scores[7].raw_score = 0.0;
    } else if (battery > max_battery) {
        char msg[QX_VIOLATION_MESSAGE_MAX];
        std::snprintf(msg, sizeof(msg),
            "Battery drain %.2f%% exceeds limit %.2f%%",
            battery, max_battery);
        push_violation(report, QX_LAW_ECONOMY, QX_SEVERITY_CRITICAL,
            msg, "batteryDrain", false);
        if (report->law_scores[7].raw_score > 50.0) {
            report->law_scores[7].raw_score = 50.0;
        }
    } else if (battery > max_battery * 0.75) {
        push_violation(report, QX_LAW_ECONOMY, QX_SEVERITY_WARNING,
            "Battery drain approaching limit", "batteryDrain", false);
        if (report->law_scores[7].raw_score > 75.0) {
            report->law_scores[7].raw_score = 75.0;
        }
    }

    const double network = input->network_redundancy_pct;
    if (network > max_network * 1.5) {
        char msg[QX_VIOLATION_MESSAGE_MAX];
        std::snprintf(msg, sizeof(msg),
            "Network redundancy %.2f%% exceeds 150%% of limit %.2f%%",
            network, max_network);
        push_violation(report, QX_LAW_ECONOMY, QX_SEVERITY_FATAL,
            msg, "networkRedundancy", false);
        report->law_scores[7].raw_score = 0.0;
    } else if (network > max_network) {
        char msg[QX_VIOLATION_MESSAGE_MAX];
        std::snprintf(msg, sizeof(msg),
            "Network redundancy %.2f%% exceeds limit %.2f%%",
            network, max_network);
        push_violation(report, QX_LAW_ECONOMY, QX_SEVERITY_CRITICAL,
            msg, "networkRedundancy", false);
        if (report->law_scores[7].raw_score > 50.0) {
            report->law_scores[7].raw_score = 50.0;
        }
    } else if (network > max_network * 0.75) {
        push_violation(report, QX_LAW_ECONOMY, QX_SEVERITY_WARNING,
            "Network redundancy approaching limit",
            "networkRedundancy", false);
        if (report->law_scores[7].raw_score > 75.0) {
            report->law_scores[7].raw_score = 75.0;
        }
    }
}

static void aggregate_scores(QXLawReport *report) noexcept {
    double health = 0.0;
    for (uint32_t i = 0u; i < QX_LAW_COUNT; ++i) {
        report->law_scores[i].weighted_score =
            report->law_scores[i].raw_score * QX_LAW_WEIGHTS[i];
        health += report->law_scores[i].weighted_score;
    }
    report->health_score   = health;
    report->certification  =
        certification_from_score(health);

    report->is_fully_compliant    = QX_TRUE;
    report->has_fatal_violation   = QX_FALSE;
    report->has_critical_violation = QX_FALSE;

    for (uint32_t i = 0u; i < report->violation_count; ++i) {
        const QXSeverityId sev = report->violations[i].severity;
        if (sev == QX_SEVERITY_FATAL) {
            report->has_fatal_violation    = QX_TRUE;
            report->is_fully_compliant     = QX_FALSE;
        }
        if (sev >= QX_SEVERITY_CRITICAL) {
            report->has_critical_violation = QX_TRUE;
            report->is_fully_compliant     = QX_FALSE;
        }
        if (sev >= QX_SEVERITY_WARNING) {
            report->is_fully_compliant     = QX_FALSE;
        }
    }
}

} // anonymous namespace

void law_enforcer_evaluate_all(
    const QXLawEvaluationInput *input,
    const QXEnforcerConfig     *config,
    QXLawReport                *report
) noexcept {
    evaluate_law1(input, report);
    evaluate_law2(input, report);
    evaluate_law3(input, report);
    evaluate_law4(input, report);
    evaluate_law5(input, config, report);
    evaluate_law6(input, report);
    evaluate_law7(input, config, report);
    evaluate_law8(input, config, report);
    aggregate_scores(report);
}

void law_enforcer_evaluate_single(
    QXLawId                      law_id,
    const QXLawEvaluationInput  *input,
    const QXEnforcerConfig      *config,
    QXLawReport                 *report
) noexcept {
    switch (law_id) {
        case QX_LAW_PATTERN:
            evaluate_law1(input, report); break;
        case QX_LAW_LIMIT:
            evaluate_law2(input, report); break;
        case QX_LAW_PAIRS:
            evaluate_law3(input, report); break;
        case QX_LAW_EQUILIBRIUM:
            evaluate_law4(input, report); break;
        case QX_LAW_KNOWLEDGE:
            evaluate_law5(input, config, report); break;
        case QX_LAW_ETHICS:
            evaluate_law6(input, report); break;
        case QX_LAW_CREATIVITY:
            evaluate_law7(input, config, report); break;
        case QX_LAW_ECONOMY:
            evaluate_law8(input, config, report); break;
        default: break;
    }
    aggregate_scores(report);
}

} // namespace qx::internal
