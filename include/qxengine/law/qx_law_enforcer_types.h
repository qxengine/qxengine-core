// =============================================================================
// QXEngine Core – include/qxengine/law/qx_law_enforcer_types.h
// Owner : Masa Bayu
// Created: 2026-05-24
// Repo   : https://github.com/qxengine/qxengine-core
// Description: Enforcer configuration and aggregate statistics types.
//              Consumed by qx_law_enforcer.h and any subsystem that needs
//              to inspect enforcer state without pulling in the full API.
// =============================================================================

#ifndef QXENGINE_LAW_QX_LAW_ENFORCER_TYPES_H
#define QXENGINE_LAW_QX_LAW_ENFORCER_TYPES_H

#include "qxengine/core/qx_types.h"
#include "qxengine/core/qx_constants.h"
#include "qxengine/law/qx_law_types.h"
#include "qxengine/law/qx_law_report.h"

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Constants
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Maximum number of QXLawReports retained in the enforcer audit trail.
 *
 * When the history is full the oldest report is overwritten (ring buffer).
 * Value mirrors QX_VIOLATION_LOG_CAP from qx_constants.h.
 */
#define QX_ENFORCER_HISTORY_CAP     1000u

/**
 * @brief Minimum health score required to pass constitutional certification.
 *
 * Below this threshold the engine must not be considered production-ready.
 * Mirrors QX_CERT_STANDARD_MIN_SCORE from qx_constants.h.
 */
#define QX_ENFORCER_MIN_PASS_SCORE  60.0

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXEnforcerConfig
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Configuration passed to qx_law_enforcer_create().
 *
 * Allows the caller to override evaluation thresholds declared in the
 * manifest without recompiling the core. All fields are optional; set to
 * zero/false to use the compile-time defaults from qx_constants.h.
 */
typedef struct QXEnforcerConfig {
    /**
     * @brief Minimum knowledge score threshold for Law 5.
     *
     * Must be in range [0.0, 100.0]. 0.0 → use QX_MIN_KNOWLEDGE_SCORE (90.0).
     */
    double      min_knowledge_score;

    /**
     * @brief Minimum native utilisation ratio for Law 7.
     *
     * Must be in range [0.0, 1.0]. 0.0 → use QX_MIN_NATIVE_UTILISATION (0.5).
     */
    double      min_native_utilisation;

    /**
     * @brief Maximum battery drain percentage per 10-minute window for Law 8.
     *
     * 0.0 → use QX_MAX_BATTERY_DRAIN_PCT (10.0).
     */
    double      max_battery_drain_pct;

    /**
     * @brief Maximum network redundancy percentage for Law 8.
     *
     * 0.0 → use QX_MAX_NETWORK_REDUNDANCY (10.0).
     */
    double      max_network_redundancy_pct;

    /**
     * @brief Overload threshold for Law 4 (Equilibrium).
     *
     * Segment utilisation above this value is considered overloaded.
     * 0.0 → use QX_EQUILIBRIUM_OVERLOAD_PCT (80.0).
     */
    double      equilibrium_overload_pct;

    /**
     * @brief Starvation threshold for Law 4 (Equilibrium).
     *
     * Segment utilisation below this value is considered starved.
     * 0.0 → use QX_EQUILIBRIUM_STARVATION_PCT (20.0).
     */
    double      equilibrium_starvation_pct;

    /**
     * @brief Minimum pairs ratio for Law 3.
     *
     * Below this value a WARNING is raised for that segment.
     * 0.0 → use QX_MIN_PAIRS_RATIO (0.5).
     */
    double      min_pairs_ratio;

    /**
     * @brief If QX_TRUE, a FATAL violation causes evaluate() to return
     *        QX_ERR_LAW_* immediately after committing the report.
     *
     * If QX_FALSE (default), evaluate() always returns QX_OK and the
     * caller inspects the report's has_fatal_violation flag directly.
     * Recommended: QX_FALSE for production; QX_TRUE for test suites.
     */
    QXBool      fail_fast_on_fatal;

    /**
     * @brief If QX_TRUE, evaluation timing is measured and stored in
     *        QXLawReport.evaluation_duration_us.
     *
     * Adds a small overhead (~1 µs). Recommended: QX_TRUE always.
     */
    QXBool      measure_duration;
} QXEnforcerConfig;

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXEnforcerStats
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Aggregate lifetime statistics for a QXLawEnforcer instance.
 */
typedef struct QXEnforcerStats {
    /** Total number of successful evaluation cycles since creation. */
    uint64_t        total_evaluations;

    /** Number of evaluations that produced at least one FATAL violation. */
    uint64_t        fatal_evaluations;

    /** Number of evaluations that produced at least one CRITICAL violation. */
    uint64_t        critical_evaluations;

    /** Number of evaluations where all eight laws were fully compliant. */
    uint64_t        fully_compliant_evaluations;

    /** Highest health score ever achieved (0.0 – 100.0). */
    double          peak_health_score;

    /** Lowest health score ever recorded (0.0 – 100.0). */
    double          trough_health_score;

    /** Average health score across all evaluations. */
    double          mean_health_score;

    /** Most recent composite health score. */
    double          last_health_score;

    /** Most recent certification tier. */
    QXCertificationId last_certification;

    /**
     * @brief Per-law violation counts since creation.
     *
     * Index 0 → Law 1 (Pattern), index 7 → Law 8 (Economy).
     */
    uint64_t        law_violation_counts[QX_LAW_COUNT];

    /**
     * @brief Per-law FATAL violation counts since creation.
     *
     * Index 0 → Law 1 (Pattern), index 7 → Law 8 (Economy).
     */
    uint64_t        law_fatal_counts[QX_LAW_COUNT];

    /** Number of reports currently stored in the audit history. */
    uint32_t        history_count;

    /** Timestamp of the first evaluation ever performed (ms). */
    QXTimestamp     first_evaluated_ms;

    /** Timestamp of the most recent evaluation (ms). */
    QXTimestamp     last_evaluated_ms;

    /**
     * @brief Average evaluation duration across all cycles (microseconds).
     *
     * Only meaningful when QXEnforcerConfig.measure_duration == QX_TRUE.
     */
    uint64_t        mean_evaluation_duration_us;
} QXEnforcerStats;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* QXENGINE_LAW_QX_LAW_ENFORCER_TYPES_H */
