// =============================================================================
// QXEngine Core – include/qxengine/law/qx_law_enforcer.h
// Owner : Masa Bayu
// Created: 2026-05-24
// Repo   : https://github.com/qxengine/qxengine-core
// Description: C ABI for the QXLawEnforcer – the constitutional evaluation
//              engine that validates all eight laws on every cycle, produces
//              a structured QXLawReport, and maintains an immutable audit
//              trail of every report ever generated.
//
// Responsibilities:
//   • Accept a QXLawEvaluationInput snapshot from the platform adapter.
//   • Evaluate each of the eight constitutional laws independently.
//   • Compute per-law raw scores, weighted scores, and worst severity.
//   • Produce a composite health score (0.0 – 100.0).
//   • Assign a QXCertificationId tier to each report.
//   • Append the report to a bounded report history (max 1 000 entries).
//   • Expose the full report history for telemetry export.
//   • Never mutate a report once it has been committed to history.
//
// Evaluation Guarantees:
//   • Deterministic: identical inputs always produce identical reports.
//   • Non-blocking: no heap allocation occurs during the evaluation pass.
//   • Thread-safe: evaluate() is serialised via an internal reader-writer lock.
//   • Audit immutability: committed reports cannot be modified or deleted
//     until qx_law_enforcer_clear_history() is called explicitly.
//
// Thread Safety:
//   qx_law_enforcer_create() and qx_law_enforcer_destroy() are NOT
//   thread-safe. All other functions are thread-safe.
//
// Repository: https://github.com/qxengine/qxengine-core
// =============================================================================

#ifndef QXENGINE_LAW_QX_LAW_ENFORCER_H
#define QXENGINE_LAW_QX_LAW_ENFORCER_H

#include "qxengine/core/qx_error.h"
#include "qxengine/law/qx_law_enforcer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Create a new QXLawEnforcer instance.
 *
 * Allocates all internal structures including the report ring buffer
 * (QX_ENFORCER_HISTORY_CAP × sizeof(QXLawReport)).
 *
 * @param config        Optional configuration. Pass NULL to use all defaults.
 * @param out_enforcer  On success, receives the new enforcer handle.
 * @return QX_OK on success.
 * @return QX_ERR_INVALID_ARGUMENT if out_enforcer is NULL.
 * @return QX_ERR_INVALID_ARGUMENT if config contains out-of-range values.
 * @return QX_ERR_INTERNAL on memory allocation failure.
 *
 * @note Not thread-safe. Call from the owning thread only.
 */
QX_API QXResult qx_law_enforcer_create(
    const QXEnforcerConfig  *config,
    QXLawEnforcerHandle     *out_enforcer
);

/**
 * @brief Destroy a QXLawEnforcer and release all internal resources.
 *
 * All report history is discarded. After this call the handle is invalid.
 *
 * @param enforcer  Handle returned by qx_law_enforcer_create(). No-op if NULL.
 *
 * @note Not thread-safe. Ensure no concurrent calls are in progress.
 */
QX_API void qx_law_enforcer_destroy(
    QXLawEnforcerHandle enforcer
);

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Evaluation
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Evaluate all eight constitutional laws and produce a QXLawReport.
 *
 * Evaluation order (fixed, non-negotiable):
 *   1. Law 1 – Pattern      (label validation)
 *   2. Law 2 – Limit        (budget thresholds per segment)
 *   3. Law 3 – Pairs        (allocation / deallocation balance)
 *   4. Law 4 – Equilibrium  (cross-segment balance)
 *   5. Law 5 – Knowledge    (cognitive snapshot freshness and score)
 *   6. Law 6 – Ethics       (manifest ethics flags)
 *   7. Law 7 – Creativity   (native capability utilisation)
 *   8. Law 8 – Economy      (battery drain and network redundancy)
 *
 * The resulting QXLawReport is written to *out_report (if non-NULL),
 * appended to the internal audit history, and assigned a monotonically
 * increasing sequence number.
 *
 * @param enforcer    Non-NULL enforcer handle.
 * @param input       Non-NULL pointer to a fully populated QXLawEvaluationInput.
 * @param out_report  Optional. If non-NULL, receives a copy of the report.
 * @return QX_OK on successful evaluation.
 * @return QX_ERR_NULL_HANDLE if enforcer is NULL.
 * @return QX_ERR_INVALID_ARGUMENT if input is NULL or segment_count is invalid.
 * @return QX_ERR_LAW_PATTERN…QX_ERR_LAW_ECONOMY if fail_fast is active and
 *         the corresponding law produced a FATAL violation.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_law_enforcer_evaluate(
    QXLawEnforcerHandle             enforcer,
    const QXLawEvaluationInput     *input,
    QXLawReport                    *out_report
);

/**
 * @brief Evaluate a single law in isolation and return a partial report.
 *
 * Useful for targeted diagnostics and unit testing. The partial report
 * contains only the QXLawScore for the requested law; all other scores
 * are zero-initialised. The partial report is NOT committed to history.
 *
 * @param enforcer    Non-NULL enforcer handle.
 * @param law_id      Law to evaluate (QX_LAW_PATTERN through QX_LAW_ECONOMY).
 * @param input       Non-NULL pointer to a QXLawEvaluationInput.
 * @param out_report  Non-NULL. Receives the partial report.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if enforcer or out_report is NULL.
 * @return QX_ERR_INVALID_ARGUMENT if input is NULL or law_id is out of range.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_law_enforcer_evaluate_single(
    QXLawEnforcerHandle             enforcer,
    QXLawId                         law_id,
    const QXLawEvaluationInput     *input,
    QXLawReport                    *out_report
);

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Report History
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Return the number of reports currently in the audit history.
 *
 * @param enforcer   Non-NULL enforcer handle.
 * @param out_count  Receives the count (0 – QX_ENFORCER_HISTORY_CAP).
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if enforcer or out_count is NULL.
 */
QX_API QXResult qx_law_enforcer_report_count(
    QXLawEnforcerHandle  enforcer,
    uint32_t            *out_count
);

/**
 * @brief Retrieve a report from the audit history by logical index.
 *
 * Index 0 is the oldest report; index (count - 1) is the most recent.
 *
 * @param enforcer    Non-NULL enforcer handle.
 * @param index       Zero-based logical index into the history.
 * @param out_report  Non-NULL. Receives a copy of the report.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if enforcer or out_report is NULL.
 * @return QX_ERR_INVALID_ARGUMENT if index ≥ current report count.
 */
QX_API QXResult qx_law_enforcer_report_at(
    QXLawEnforcerHandle  enforcer,
    uint32_t             index,
    QXLawReport         *out_report
);

/**
 * @brief Retrieve the most recent report from the audit history.
 *
 * @param enforcer    Non-NULL enforcer handle.
 * @param out_report  Non-NULL. Receives a copy of the latest report.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if enforcer or out_report is NULL.
 * @return QX_ERR_INVALID_ARGUMENT if the history is empty.
 */
QX_API QXResult qx_law_enforcer_latest_report(
    QXLawEnforcerHandle  enforcer,
    QXLawReport         *out_report
);

/**
 * @brief Bulk-copy up to @p capacity reports from the history, oldest-first.
 *
 * @param enforcer      Non-NULL enforcer handle.
 * @param out_reports   Caller-allocated array of QXLawReport.
 * @param capacity      Number of elements in @p out_reports.
 * @param out_written   Receives the number of reports actually written.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if any pointer is NULL.
 * @return QX_ERR_INVALID_ARGUMENT if capacity == 0.
 */
QX_API QXResult qx_law_enforcer_reports_copy(
    QXLawEnforcerHandle  enforcer,
    QXLawReport         *out_reports,
    uint32_t             capacity,
    uint32_t            *out_written
);

/**
 * @brief Find the most recent report containing a violation for a given law.
 *
 * Searches newest-first and returns the first matching report.
 *
 * @param enforcer    Non-NULL enforcer handle.
 * @param law_id      Law to search for.
 * @param out_report  Non-NULL. Receives a copy of the matching report.
 * @return QX_OK if a matching report was found.
 * @return QX_ERR_NULL_HANDLE if enforcer or out_report is NULL.
 * @return QX_ERR_INVALID_ARGUMENT if law_id is out of range or history empty.
 * @return QX_ERR_NOT_SUPPORTED if no report contains a violation for law_id.
 */
QX_API QXResult qx_law_enforcer_find_violation_report(
    QXLawEnforcerHandle  enforcer,
    QXLawId              law_id,
    QXLawReport         *out_report
);

/**
 * @brief Clear all reports from the audit history.
 *
 * The sequence counter is NOT reset; audit continuity is preserved.
 *
 * @param enforcer  Non-NULL enforcer handle.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if enforcer is NULL.
 */
QX_API QXResult qx_law_enforcer_clear_history(
    QXLawEnforcerHandle enforcer
);

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Statistics & Health Queries
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Return aggregate lifetime statistics for the enforcer.
 *
 * @param enforcer   Non-NULL enforcer handle.
 * @param out_stats  Receives a snapshot of the enforcer's aggregate stats.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if enforcer or out_stats is NULL.
 */
QX_API QXResult qx_law_enforcer_stats(
    QXLawEnforcerHandle  enforcer,
    QXEnforcerStats     *out_stats
);

/**
 * @brief Return the composite health score from the most recent evaluation.
 *
 * @param enforcer    Non-NULL enforcer handle.
 * @param out_score   Receives the health score (0.0 – 100.0).
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if enforcer or out_score is NULL.
 * @return QX_ERR_INVALID_ARGUMENT if no evaluation has been performed yet.
 */
QX_API QXResult qx_law_enforcer_health_score(
    QXLawEnforcerHandle  enforcer,
    double              *out_score
);

/**
 * @brief Return the certification tier from the most recent evaluation.
 *
 * @param enforcer          Non-NULL enforcer handle.
 * @param out_certification Receives the certification tier.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if enforcer or out_certification is NULL.
 * @return QX_ERR_INVALID_ARGUMENT if no evaluation has been performed yet.
 */
QX_API QXResult qx_law_enforcer_certification(
    QXLawEnforcerHandle  enforcer,
    QXCertificationId   *out_certification
);

/**
 * @brief Return QX_TRUE if the most recent evaluation was fully compliant.
 *
 * @param enforcer      Non-NULL enforcer handle.
 * @param out_compliant Receives QX_TRUE if compliant, QX_FALSE otherwise.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if enforcer or out_compliant is NULL.
 * @return QX_ERR_INVALID_ARGUMENT if no evaluation has been performed yet.
 */
QX_API QXResult qx_law_enforcer_is_compliant(
    QXLawEnforcerHandle  enforcer,
    QXBool              *out_compliant
);

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Certification Helpers
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Return a human-readable label for a QXCertificationId.
 *
 * @return "Sovereign", "Professional", "Standard", "Invalid", or "Unknown".
 * @note Returns a string literal; do not free.
 */
QX_API const char *qx_certification_label(QXCertificationId cert);

/**
 * @brief Return the minimum health score required for a certification tier.
 *
 * QX_CERT_SOVEREIGN → 95.0, QX_CERT_PROFESSIONAL → 80.0,
 * QX_CERT_STANDARD  → 60.0, QX_CERT_INVALID      →  0.0
 */
QX_API double qx_certification_min_score(QXCertificationId cert);

/**
 * @brief Derive the QXCertificationId for a given health score.
 *
 * ≥ 95.0 → SOVEREIGN, ≥ 80.0 → PROFESSIONAL,
 * ≥ 60.0 → STANDARD,  < 60.0 → INVALID.
 */
QX_API QXCertificationId qx_certification_from_score(double health_score);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* QXENGINE_LAW_QX_LAW_ENFORCER_H */
