// =============================================================================
// QXEngine Core – include/qxengine/law/qx_law_types.h
// Owner : Masa Bayu
// Created: 2026-05-24
// Repo   : https://github.com/qxengine/qxengine-core
// Description: Identity layer – constitutional law constants, weight table,
//              violation severity levels, and the QXViolation record.
//              Consumed by both the law enforcer and the telemetry subsystem.
//
// Constitutional Laws (immutable – spec v1.0.0):
//   Law 1  – Pattern    : every allocation must carry a valid non-blank label
//                         (length 3–128 chars, UTF-8).
//   Law 2  – Limit      : segment budgets must never exceed the manifest hard
//                         limit; soft limit triggers a warning.
//   Law 3  – Pairs      : every allocation must be paired with a deallocation;
//                         orphaned leaves degrade the pairs ratio.
//   Law 4  – Equilibrium: no single segment may be overloaded (> 80 %) while
//                         another is starved (< 20 %) simultaneously.
//   Law 5  – Knowledge  : the engine must maintain a minimum knowledge score
//                         of 90.0 via regular cognitive snapshots.
//   Law 6  – Ethics     : all manifest ethics flags must be active (dark
//                         patterns, deceptive flows, and manipulative UX
//                         prohibited; privacy-first design enforced).
//   Law 7  – Creativity : native capabilities must be declared and utilised
//                         at or above the manifest native-utilisation target.
//   Law 8  – Economy    : battery drain and network redundancy must remain
//                         within manifest-declared thresholds.
//
// Severity Levels:
//   INFO     – observation, no corrective action required.
//   WARNING  – approaching a constitutional threshold; corrective action advised.
//   CRITICAL – threshold breached; engine health degraded.
//   FATAL    – constitutional law violated; engine must halt or refuse operation.
//
// Repository: https://github.com/qxengine/qxengine-core
// =============================================================================

#ifndef QXENGINE_LAW_QX_LAW_TYPES_H
#define QXENGINE_LAW_QX_LAW_TYPES_H

#include "qxengine/core/qx_types.h"
#include "qxengine/core/qx_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Law Count & Weight Table
// ─────────────────────────────────────────────────────────────────────────────
//
// Each law contributes a weighted score to the composite engine health score.
// Weights are authoritative (spec v1.0.0) and must sum to exactly 1.0.
//
//   Law 1  Pattern      0.15
//   Law 2  Limit        0.20
//   Law 3  Pairs        0.15
//   Law 4  Equilibrium  0.10
//   Law 5  Knowledge    0.15
//   Law 6  Ethics       0.10
//   Law 7  Creativity   0.10
//   Law 8  Economy      0.05
//                       ────
//   Total               1.00
//
// A law's weighted contribution is zero when that law is violated at FATAL
// severity. At CRITICAL severity the weight contribution is halved.
// ─────────────────────────────────────────────────────────────────────────────

/** Number of constitutional laws (immutable). */
#define QX_LAW_COUNT    8u

/**
 * @brief Per-law weight used in health-score computation.
 *
 * Index 0 → Law 1 (Pattern), index 7 → Law 8 (Economy).
 * Values are compile-time constants matching the spec; do not modify.
 */
static const double QX_LAW_WEIGHTS[QX_LAW_COUNT] = {
    0.15,   /* Law 1 – Pattern      */
    0.20,   /* Law 2 – Limit        */
    0.15,   /* Law 3 – Pairs        */
    0.10,   /* Law 4 – Equilibrium  */
    0.15,   /* Law 5 – Knowledge    */
    0.10,   /* Law 6 – Ethics       */
    0.10,   /* Law 7 – Creativity   */
    0.05    /* Law 8 – Economy      */
};

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Violation String Length Constants
// ─────────────────────────────────────────────────────────────────────────────

/** Maximum length of a violation error code string (e.g. "LAW_2_LIMIT-FATAL"). */
#define QX_VIOLATION_CODE_MAX       32u

/** Maximum length of a human-readable violation message. */
#define QX_VIOLATION_MESSAGE_MAX    256u

/** Maximum length of the detail field (contextual data, e.g. segment ID). */
#define QX_VIOLATION_DETAIL_MAX     128u

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXViolation
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief A single constitutional-law violation record.
 *
 * Produced by the QXLawEnforcer during every evaluation cycle and written
 * to the telemetry violation log (capped at QX_VIOLATION_LOG_CAP = 1 000).
 */
typedef struct QXViolation {
    /**
     * @brief The constitutional law that was violated.
     *
     * Valid range: QX_LAW_PATTERN (1) through QX_LAW_ECONOMY (8).
     */
    QXLawId         law_id;

    /**
     * @brief Severity of this violation.
     *
     * Determines corrective action:
     *   INFO     → log only.
     *   WARNING  → log + notify platform adapter.
     *   CRITICAL → log + notify + degrade health score.
     *   FATAL    → log + notify + zero that law's health contribution.
     */
    QXSeverityId    severity;

    /**
     * @brief Structured error code string.
     *
     * Format: "LAW_<N>_<NAME>-<SEVERITY>"
     * Examples:
     *   "LAW_1_PATTERN-FATAL"
     *   "LAW_2_LIMIT-CRITICAL"
     *   "LAW_6_ETHICS-FATAL"
     * Null-terminated. Max QX_VIOLATION_CODE_MAX bytes including null.
     */
    char            error_code[QX_VIOLATION_CODE_MAX];

    /**
     * @brief Human-readable description of the violation.
     *
     * Null-terminated. Max QX_VIOLATION_MESSAGE_MAX bytes including null.
     * Example: "Segment QLM-IMG hard budget exceeded by 14.3 MB"
     */
    char            message[QX_VIOLATION_MESSAGE_MAX];

    /**
     * @brief Optional contextual detail (segment ID, leaf label, value, etc.).
     *
     * Null-terminated. Max QX_VIOLATION_DETAIL_MAX bytes including null.
     * Empty string when no detail is available.
     */
    char            detail[QX_VIOLATION_DETAIL_MAX];

    /**
     * @brief Wall-clock timestamp when this violation was detected
     *        (ms since Unix epoch).
     */
    QXTimestamp     detected_at_ms;

    /**
     * @brief Whether this violation was already present in the previous
     *        evaluation cycle.
     *
     * QX_TRUE  → recurring (not cleared since last cycle).
     * QX_FALSE → new (first occurrence in this cycle).
     */
    QXBool          is_recurring;
} QXViolation;


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* QXENGINE_LAW_QX_LAW_TYPES_H */
