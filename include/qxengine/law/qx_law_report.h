// =============================================================================
// QXEngine Core – include/qxengine/law/qx_law_report.h
// Owner : Masa Bayu
// Created: 2026-05-24
// Repo   : https://github.com/qxengine/qxengine-core
// Description: Evaluation layer – per-segment input snapshot, full evaluation
//              input, per-law score, structured health report, and inline
//              convenience predicates.
//              Depends on qx_law_types.h for QXViolation and weight constants.
// =============================================================================

#ifndef QXENGINE_LAW_QX_LAW_REPORT_H
#define QXENGINE_LAW_QX_LAW_REPORT_H

#include "qxengine/law/qx_law_types.h"
#include "qxengine/intelligence/qx_snapshot_types.h"
#include "qxengine/memory/qx_segment.h"

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Evaluation Input Constants
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Maximum number of segments whose stats can be passed to an
 *        evaluation cycle. Must equal QX_SEGMENT_COUNT (9).
 */
#define QX_EVAL_MAX_SEGMENTS        9u

/** Maximum number of native capability IDs in an evaluation input. */
#define QX_EVAL_MAX_CAPABILITIES    32u

/** Maximum length of a native capability ID string. */
#define QX_CAPABILITY_ID_MAX        64u

/** Maximum number of violations a single report can hold. */
#define QX_REPORT_MAX_VIOLATIONS    64u

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXLawSegmentInput
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Per-segment data snapshot used as input to the law evaluator.
 *
 * Mirrors QXSegmentStats but uses only the fields required by the eight laws,
 * keeping the evaluation layer decoupled from the memory subsystem headers.
 */
typedef struct QXLawSegmentInput {
    /** Null-terminated segment ID (e.g. "QLM-IMG"). Max 16 bytes. */
    char        segment_id[16];

    /** Bytes currently allocated in this segment. */
    QXSize      used_bytes;

    /** Soft budget limit in bytes (from manifest, scaled to device class). */
    QXSize      soft_limit_bytes;

    /** Hard budget limit in bytes (from manifest, scaled to device class). */
    QXSize      hard_limit_bytes;

    /** Total allocation calls since engine start. */
    uint64_t    total_allocations;

    /** Total deallocation calls since engine start. */
    uint64_t    total_deallocations;

    /** Bytes currently classified as orphaned (idle > QX_ORPHAN_IDLE_SEC). */
    QXSize      orphaned_bytes;

    /**
     * @brief Pairs ratio: total_deallocations / total_allocations.
     *
     * Range [0.0, 1.0]. Below QX_MIN_PAIRS_RATIO (0.5) triggers Law 3 WARNING.
     * Zero when total_allocations == 0 (no violation inferred).
     */
    double      pairs_ratio;

    /**
     * @brief Hard-limit utilisation: used_bytes / hard_limit_bytes.
     *
     * Range [0.0, ∞). > 1.0 means budget exceeded → Law 2 FATAL.
     */
    double      hard_utilisation;

    /**
     * @brief Soft-limit utilisation: used_bytes / soft_limit_bytes.
     *
     * Range [0.0, ∞). > 1.0 means soft budget breached → Law 2 WARNING.
     */
    double      soft_utilisation;
} QXLawSegmentInput;

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXLawEvaluationInput
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Full input snapshot for a single law-enforcement evaluation cycle.
 *
 * The platform adapter (QXEngine.swift / QXEngine.kt) collects this data
 * from all subsystems and passes it to qx_law_enforcer_evaluate().
 */
typedef struct QXLawEvaluationInput {
    // ── Law 1 – Pattern ─────────────────────────────────────────────────────

    /**
     * @brief Number of allocation attempts with a blank or invalid label
     *        detected since the last evaluation cycle.
     *
     * Any value > 0 triggers a Law 1 FATAL violation.
     */
    uint32_t    unlabelled_allocation_count;

    // ── Laws 2, 3, 4 – Limit, Pairs, Equilibrium ────────────────────────────

    /**
     * @brief Per-segment input data. Must provide exactly segment_count
     *        entries covering all active segments.
     */
    QXLawSegmentInput   segments[QX_EVAL_MAX_SEGMENTS];

    /** Number of valid entries in @p segments. Must be ≤ QX_EVAL_MAX_SEGMENTS. */
    uint32_t            segment_count;

    // ── Law 5 – Knowledge ────────────────────────────────────────────────────

    /**
     * @brief Current cognitive knowledge score (0.0 – 100.0).
     *
     * Below QX_MIN_KNOWLEDGE_SCORE (90.0) triggers a Law 5 violation:
     *   < 90.0  → WARNING
     *   < 70.0  → CRITICAL
     *   < 50.0  → FATAL
     */
    double      knowledge_score;

    /**
     * @brief Seconds elapsed since the last successful cognitive snapshot.
     *
     * Exceeding QX_SNAPSHOT_INTERVAL_SEC triggers a Law 5 WARNING.
     */
    double      seconds_since_last_snapshot;

    // ── Law 6 – Ethics ───────────────────────────────────────────────────────

    /** Dark patterns prohibited flag from manifest. Must be QX_TRUE. */
    QXBool      dark_patterns_prohibited;

    /** Deceptive flows prohibited flag from manifest. Must be QX_TRUE. */
    QXBool      deceptive_flows_prohibited;

    /** Manipulative UX prohibited flag from manifest. Must be QX_TRUE. */
    QXBool      manipulative_ux_prohibited;

    /** Privacy-first design flag from manifest. Must be QX_TRUE. */
    QXBool      privacy_first_design;

    /** Transparent data usage flag from manifest. Must be QX_TRUE. */
    QXBool      transparent_data_usage;

    // ── Law 7 – Creativity ───────────────────────────────────────────────────

    /** Native-first policy flag from manifest. Must be QX_TRUE. */
    QXBool      native_first_policy;

    /**
     * @brief Measured native-capability utilisation ratio (0.0 – 1.0).
     *
     * Below the manifest nativeUtilisationTarget triggers Law 7 WARNING.
     * Below QX_MIN_NATIVE_UTILISATION (0.5) regardless of target → CRITICAL.
     * native_first_policy == QX_FALSE → FATAL (no utilisation check performed).
     */
    double      native_utilisation_ratio;

    /**
     * @brief Null-terminated IDs of native capabilities declared in manifest.
     *
     * Max QX_EVAL_MAX_CAPABILITIES entries. Max QX_CAPABILITY_ID_MAX per ID.
     */
    char        declared_capabilities[QX_EVAL_MAX_CAPABILITIES][QX_CAPABILITY_ID_MAX];

    /** Number of valid entries in @p declared_capabilities. */
    uint32_t    declared_capability_count;

    /**
     * @brief Null-terminated IDs of capabilities confirmed active at runtime.
     *
     * Must be a subset of declared_capabilities. Any declared capability that
     * is not active contributes to native utilisation ratio degradation.
     */
    char        active_capabilities[QX_EVAL_MAX_CAPABILITIES][QX_CAPABILITY_ID_MAX];

    /** Number of valid entries in @p active_capabilities. */
    uint32_t    active_capability_count;

    // ── Law 8 – Economy ──────────────────────────────────────────────────────

    /**
     * @brief Measured battery drain as a percentage per 10-minute window.
     *
     * Thresholds (from QX_MAX_BATTERY_DRAIN_PCT = 10.0):
     *   > 75 % of limit (7.5 %)  → WARNING
     *   > 100 % of limit (10.0 %) → CRITICAL
     *   > 150 % of limit (15.0 %) → FATAL
     */
    double      battery_drain_pct_per_10min;

    /**
     * @brief Measured network redundancy as a percentage of total traffic.
     *
     * Thresholds (from QX_MAX_NETWORK_REDUNDANCY = 10.0):
     *   > 75 % of limit (7.5 %)  → WARNING
     *   > 100 % of limit (10.0 %) → CRITICAL
     *   > 150 % of limit (15.0 %) → FATAL
     */
    double      network_redundancy_pct;

    /** Wall-clock timestamp when this input snapshot was captured (ms). */
    QXTimestamp captured_at_ms;
} QXLawEvaluationInput;

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXLawScore
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Per-law score entry within a QXLawReport.
 */
typedef struct QXLawScore {
    /** The law this entry describes. */
    QXLawId         law_id;

    /**
     * @brief Raw law score before weighting (0.0 – 100.0).
     *
     * 100.0 → fully compliant, no violations detected.
     *   0.0 → FATAL violation, zero contribution to health score.
     */
    double          raw_score;

    /**
     * @brief Weighted contribution to the composite health score.
     *
     * weighted_score = raw_score × QX_LAW_WEIGHTS[law_id - 1]
     */
    double          weighted_score;

    /** Highest severity violation detected for this law in this cycle. */
    QXSeverityId    worst_severity;

    /** Number of violations detected for this law in this cycle. */
    uint32_t        violation_count;

    /** QX_TRUE if this law is fully compliant (zero violations). */
    QXBool          is_compliant;
} QXLawScore;

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXLawReport
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Full evaluation report produced by qx_law_enforcer_evaluate().
 *
 * Contains per-law scores, the composite health score, certification tier,
 * and all violations detected in the evaluation cycle.
 */
typedef struct QXLawReport {
    /** Scores for each of the eight constitutional laws (index 0 = Law 1). */
    QXLawScore      law_scores[QX_LAW_COUNT];

    /**
     * @brief Composite engine health score (0.0 – 100.0).
     *
     * Computed as:
     *   health = Σ (law_scores[i].weighted_score) for i in 0..7
     *
     * Certification tiers:
     *   ≥ 95.0  → QX_CERT_SOVEREIGN
     *   ≥ 80.0  → QX_CERT_PROFESSIONAL
     *   ≥ 60.0  → QX_CERT_STANDARD
     *   < 60.0  → QX_CERT_INVALID
     */
    double              health_score;

    /** Certification tier derived from @p health_score. */
    QXCertificationId   certification;

    /** QX_TRUE if all eight laws are fully compliant (zero violations). */
    QXBool          is_fully_compliant;

    /** QX_TRUE if any law has a FATAL violation. */
    QXBool          has_fatal_violation;

    /** QX_TRUE if any law has a CRITICAL or FATAL violation. */
    QXBool          has_critical_violation;

    /** Violations detected in this cycle (up to QX_REPORT_MAX_VIOLATIONS). */
    QXViolation     violations[QX_REPORT_MAX_VIOLATIONS];

    /** Number of valid entries in @p violations. */
    uint32_t        violation_count;

    /** Timestamp of the evaluation input used to produce this report (ms). */
    QXTimestamp     evaluated_at_ms;

    /**
     * @brief Duration of the evaluation pass in microseconds.
     *
     * Useful for performance monitoring; should typically be < 1 000 µs.
     */
    uint64_t        evaluation_duration_us;

    /**
     * @brief Monotonically increasing report sequence number.
     *
     * Starts at 1 on the first evaluation. Wraps at UINT64_MAX (not expected
     * in practice). Used for ordering and deduplication in telemetry.
     */
    QXSequence      sequence;
} QXLawReport;

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Convenience Predicates
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Return QX_TRUE if the report qualifies for Sovereign certification.
 */
static inline QXBool qx_report_is_sovereign(const QXLawReport *report) {
    return (report && report->certification == QX_CERT_SOVEREIGN)
           ? QX_TRUE : QX_FALSE;
}

/**
 * @brief Return QX_TRUE if the report qualifies for at least Standard tier.
 */
static inline QXBool qx_report_is_certified(const QXLawReport *report) {
    return (report && report->certification != QX_CERT_INVALID)
           ? QX_TRUE : QX_FALSE;
}

/**
 * @brief Return the worst severity across all violations in a report.
 *        Returns QX_SEVERITY_INFO if there are no violations.
 */
static inline QXSeverityId qx_report_worst_severity(const QXLawReport *report) {
    if (!report || report->violation_count == 0u) return QX_SEVERITY_INFO;
    QXSeverityId worst = QX_SEVERITY_INFO;
    for (uint32_t i = 0u; i < report->violation_count; ++i) {
        if (report->violations[i].severity > worst)
            worst = report->violations[i].severity;
    }
    return worst;
}

/**
 * @brief Find the first violation in a report for a specific law.
 *        Returns NULL if none found.
 */
static inline const QXViolation *qx_report_find_violation(
    const QXLawReport *report,
    QXLawId            law_id
) {
    if (!report) return nullptr;
    for (uint32_t i = 0u; i < report->violation_count; ++i) {
        if (report->violations[i].law_id == law_id)
            return &report->violations[i];
    }
    return nullptr;
}


/* ============================================================================
 * MARK: – Public C ABI – declared in qx_law_types.cpp
 * ========================================================================= */
QX_API QXResult qx_law_input_zero_init(QXLawEvaluationInput *input);
QX_API QXResult qx_law_report_zero_init(QXLawReport *report);
QX_API QXResult qx_violation_format_code(QXLawId law_id, QXSeverityId severity, char *buffer);
QX_API QXBool   qx_violation_is_fatal(const QXViolation *violation);
QX_API QXBool   qx_violation_is_critical_or_fatal(const QXViolation *violation);
QX_API double   qx_law_weight(QXLawId law_id);
QX_API double   qx_signal_weight(QXCognitiveSignal signal);
QX_API QXResult qx_law_segment_input_from_stats(const QXSegmentStats *stats, QXLawSegmentInput *out_input);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* QXENGINE_LAW_QX_LAW_REPORT_H */
