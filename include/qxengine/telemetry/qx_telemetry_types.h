// =============================================================================
// QXEngine Core – include/qxengine/telemetry/qx_telemetry_types.h
// Owner: Masa Bayu
// Created: 2026-05-24
// Description: Telemetry configuration, statistics, and report types.
//              Event types are in qx_telemetry_event_types.h.
//
// Repository: https://github.com/qxengine/qxengine-core
// =============================================================================

#ifndef QXENGINE_TELEMETRY_QX_TELEMETRY_TYPES_H
#define QXENGINE_TELEMETRY_QX_TELEMETRY_TYPES_H

#include "qxengine/core/qx_constants.h"
#include "qxengine/telemetry/qx_telemetry_event_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Violation / Report Constants
// ─────────────────────────────────────────────────────────────────────────────

/** Maximum number of violation records retained in the violation log. */
#define QX_TEL_VIOLATION_CAP        1000u

/** Maximum length of the human-readable summary in a telemetry report. */
#define QX_TEL_REPORT_SUMMARY_MAX   2048u

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXTelemetryConfig
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Configuration for a QXTelemetry instance.
 */
typedef struct QXTelemetryConfig {
    /**
     * @brief Master switch – if QX_FALSE no events are recorded.
     *
     * Maps to manifest knowledge.telemetryEnabled.
     * Default: QX_TRUE.
     */
    QXBool  enabled;

    /**
     * @brief If QX_TRUE, ALLOCATION and DEALLOCATION events are recorded.
     *
     * Disable in production if allocation volume is very high and
     * per-allocation telemetry overhead is unacceptable.
     * Default: QX_TRUE.
     */
    QXBool  record_allocations;

    /**
     * @brief If QX_TRUE, EVICTION events are recorded individually.
     *
     * Evictions within a single pressure cycle are batched into one
     * PRESSURE event when QX_FALSE.
     * Default: QX_TRUE.
     */
    QXBool  record_evictions;

    /**
     * @brief If QX_TRUE, EVALUATION events are recorded every cycle.
     *
     * Evaluation events include the full health score and certification tier.
     * Default: QX_TRUE.
     */
    QXBool  record_evaluations;

    /**
     * @brief If QX_TRUE, SNAPSHOT events are recorded every capture.
     *
     * Default: QX_TRUE.
     */
    QXBool  record_snapshots;

    /**
     * @brief If QX_TRUE, SECURITY events are recorded.
     *
     * Default: QX_TRUE.
     */
    QXBool  record_security;

    /**
     * @brief If QX_TRUE, VIOLATION events are also written to the
     *        dedicated violation log in addition to the event log.
     *
     * Default: QX_TRUE.
     */
    QXBool  separate_violation_log;

    /**
     * @brief If QX_TRUE, verbose diagnostic messages are included in
     *        event detail fields.
     *
     * Increases memory usage per event. Recommended for debug builds only.
     * Default: QX_FALSE.
     */
    QXBool  verbose;
} QXTelemetryConfig;

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXTelemetryStats
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Aggregate statistics for the lifetime of a QXTelemetry instance.
 */
typedef struct QXTelemetryStats {
    /** Total events recorded since creation (monotonically increasing). */
    uint64_t    total_events_recorded;

    /** Events currently stored in the ring buffer. */
    uint32_t    current_event_count;

    /** Violation records currently stored in the violation log. */
    uint32_t    current_violation_count;

    /** Total violations recorded since creation. */
    uint64_t    total_violations_recorded;

    /** Total FATAL violations recorded since creation. */
    uint64_t    total_fatal_violations;

    /** Total CRITICAL violations recorded since creation. */
    uint64_t    total_critical_violations;

    /** Total allocation events recorded. */
    uint64_t    total_allocation_events;

    /** Total deallocation events recorded. */
    uint64_t    total_deallocation_events;

    /** Total eviction events recorded. */
    uint64_t    total_eviction_events;

    /** Total pressure events recorded. */
    uint64_t    total_pressure_events;

    /** Total evaluation events recorded. */
    uint64_t    total_evaluation_events;

    /** Total snapshot events recorded. */
    uint64_t    total_snapshot_events;

    /** Timestamp of the first event ever recorded (ms). */
    QXTimestamp first_event_ms;

    /** Timestamp of the most recent event recorded (ms). */
    QXTimestamp last_event_ms;

    /**
     * @brief Per-law violation counts since creation.
     *
     * Index 0 → Law 1 (Pattern), index 7 → Law 8 (Economy).
     */
    uint64_t    law_violation_counts[QX_LAW_COUNT];
} QXTelemetryStats;

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXTelemetryReport
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief A structured summary report of engine telemetry activity.
 *
 * Produced by qx_telemetry_report(). Suitable for display in dashboards,
 * export to analytics pipelines, or inclusion in audit packages.
 */
typedef struct QXTelemetryReport {
    // ── Header ────────────────────────────────────────────────────────────────

    /** Timestamp when this report was generated (ms since epoch). */
    QXTimestamp     generated_at_ms;

    /** Engine health score at report generation time (0.0 – 100.0). */
    double          health_score;

    /** Certification tier at report generation time. */
    QXCertificationId certification;

    // ── Event Summary ─────────────────────────────────────────────────────────

    /** Total events recorded since engine start. */
    uint64_t        total_events;

    /** Breakdown of event counts by category (index = QXTelemetryCategory). */
    uint64_t        events_by_category[10];

    // ── Violation Summary ─────────────────────────────────────────────────────

    /** Total violations recorded since engine start. */
    uint64_t        total_violations;

    /** FATAL violation count since engine start. */
    uint64_t        fatal_violations;

    /** CRITICAL violation count since engine start. */
    uint64_t        critical_violations;

    /** WARNING violation count since engine start. */
    uint64_t        warning_violations;

    /**
     * @brief Per-law violation counts since engine start.
     *
     * Index 0 → Law 1 (Pattern), index 7 → Law 8 (Economy).
     */
    uint64_t        law_violation_counts[QX_LAW_COUNT];

    // ── Memory Summary ────────────────────────────────────────────────────────

    /** Total successful allocation events. */
    uint64_t        total_allocations;

    /** Total failed allocation events. */
    uint64_t        failed_allocations;

    /** Total deallocation events. */
    uint64_t        total_deallocations;

    /** Total eviction events. */
    uint64_t        total_evictions;

    /** Peak pressure tier reached since engine start (1–4). */
    QXTierId        peak_pressure_tier;

    // ── Intelligence Summary ──────────────────────────────────────────────────

    /** Total cognitive snapshots captured since engine start. */
    uint64_t        total_snapshots;

    /** Number of snapshots where Law 5 compliance failed. */
    uint64_t        non_compliant_snapshots;

    /** Most recent cognitive knowledge score. */
    double          last_knowledge_score;

    // ── Human-Readable Summary ────────────────────────────────────────────────

    /**
     * @brief Multi-line human-readable report text.
     *
     * Contains section headers, key metrics, and top violations.
     * Null-terminated UTF-8. Max QX_TEL_REPORT_SUMMARY_MAX bytes.
     */
    char            summary[QX_TEL_REPORT_SUMMARY_MAX];

    /** Timestamp of the oldest event included in this report (ms). */
    QXTimestamp     period_start_ms;

    /** Timestamp of the newest event included in this report (ms). */
    QXTimestamp     period_end_ms;

} QXTelemetryReport;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* QXENGINE_TELEMETRY_QX_TELEMETRY_TYPES_H */
