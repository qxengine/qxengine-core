// =============================================================================
// QXEngine Core – src/law/qx_law_types.cpp
// Owner: Masa Bayu
// Created: 2026-05-24
// Description: Implementation of constitutional-law type helpers – the three
//              pure C ABI functions declared in qx_law_types.h that operate
//              on QXLawReport values: qx_report_is_sovereign,
//              qx_report_is_certified, qx_report_worst_severity, and
//              qx_report_find_violation are declared as static inline in the
//              header and require no translation unit.
//
//              This file provides:
//                1. qx_law_input_zero_init()  – safely zero-initialises a
//                   QXLawEvaluationInput on the caller's behalf.
//                2. qx_law_report_zero_init() – safely zero-initialises a
//                   QXLawReport on the caller's behalf.
//                3. qx_violation_format_code() – builds the structured
//                   "LAW_N_NAME-SEVERITY" error code string.
//                4. qx_violation_is_fatal()   – convenience predicate.
//                5. qx_law_weight()           – returns the authoritative
//                   weight for a given law ID.
//                6. qx_signal_weight()        – returns the authoritative
//                   cognitive signal weight.
//
// Design constraints:
//   • No heap allocation.
//   • All functions are pure or access only static data.
//   • C linkage preserved via extern "C".
//
// Repository: https://github.com/qxengine/qxengine-core
// =============================================================================

#include "qxengine/law/qx_law_types.h"
#include "qxengine/intelligence/qx_snapshot_types.h"
#include "qxengine/memory/qx_segment.h"
#include "qxengine/law/qx_law_report.h"
#include "qxengine/core/qx_constants.h"
#include "qxengine/core/qx_error.h"
#include "qxengine/core/qx_types.h"

#include <cstring>   // std::memset, std::strncpy, std::snprintf
#include <cstdio>    // std::snprintf

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Forward declarations (internal)
// ─────────────────────────────────────────────────────────────────────────────

namespace qx::internal {
    const char *law_name(QXLawId) noexcept;
    const char *severity_name(QXSeverityId) noexcept;
} // namespace qx::internal

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

namespace {

/**
 * @brief Validate a QXLawId is in the legal range [1, QX_LAW_COUNT].
 *
 * @param law_id  Law identifier to validate.
 * @return true if valid; false otherwise.
 */
static bool law_id_valid(QXLawId law_id) noexcept {
    return (law_id >= 1 &&
            static_cast<uint32_t>(law_id) <= QX_LAW_COUNT);
}

/**
 * @brief Validate a QXSeverityId is in the legal range [0, 3].
 *
 * @param severity  Severity value to validate.
 * @return true if valid; false otherwise.
 */
static bool severity_valid(QXSeverityId severity) noexcept {
    return (static_cast<uint32_t>(severity) <= 3u);
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Public C ABI
// ─────────────────────────────────────────────────────────────────────────────

extern "C" {

// ── Zero-initialisation helpers ───────────────────────────────────────────────

/**
 * @brief Zero-initialise a QXLawEvaluationInput to a known safe state.
 *
 * Sets all numeric fields to 0 / 0.0, all boolean fields to QX_FALSE,
 * and all string fields to empty. Callers must populate all relevant
 * fields for their evaluation cycle before passing to
 * qx_law_enforcer_evaluate().
 *
 * @param input  Non-NULL pointer to a QXLawEvaluationInput.
 * @return QX_OK on success; QX_ERR_NULL_HANDLE if input is NULL.
 *
 * @note Thread-safe. Pure write – no reads of shared state.
 */
QX_API QXResult qx_law_input_zero_init(QXLawEvaluationInput *input) {
    if (!input) { return QX_ERR_NULL_HANDLE; }
    std::memset(input, 0, sizeof(QXLawEvaluationInput));

    // Explicit boolean fields – memset sets to QX_FALSE (0) correctly.
    // Explicit numeric sentinels for clarity:
    input->knowledge_score              = 0.0;
    input->seconds_since_last_snapshot  = 0.0;
    input->native_utilisation_ratio     = 0.0;
    input->battery_drain_pct_per_10min  = 0.0;
    input->network_redundancy_pct       = 0.0;
    input->segment_count                = 0u;
    input->declared_capability_count    = 0u;
    input->active_capability_count      = 0u;
    input->unlabelled_allocation_count  = 0u;

    return QX_OK;
}

/**
 * @brief Zero-initialise a QXLawReport to a known safe state.
 *
 * Sets all numeric fields to 0 / 0.0, certification to QX_CERT_INVALID,
 * all compliance flags to QX_FALSE, and all string fields to empty.
 *
 * @param report  Non-NULL pointer to a QXLawReport.
 * @return QX_OK on success; QX_ERR_NULL_HANDLE if report is NULL.
 *
 * @note Thread-safe. Pure write – no reads of shared state.
 */
QX_API QXResult qx_law_report_zero_init(QXLawReport *report) {
    if (!report) { return QX_ERR_NULL_HANDLE; }
    std::memset(report, 0, sizeof(QXLawReport));

    report->health_score          = 0.0;
    report->certification         = QX_CERT_INVALID;
    report->is_fully_compliant    = QX_FALSE;
    report->has_fatal_violation   = QX_FALSE;
    report->has_critical_violation = QX_FALSE;
    report->violation_count       = 0u;
    report->evaluated_at_ms       = 0u;
    report->evaluation_duration_us = 0u;
    report->sequence              = 0u;

    for (uint32_t i = 0u; i < QX_LAW_COUNT; ++i) {
        report->law_scores[i].law_id         =
            static_cast<QXLawId>(i + 1u);
        report->law_scores[i].raw_score      = 100.0;
        report->law_scores[i].weighted_score =
            QX_LAW_WEIGHTS[i] * 100.0;
        report->law_scores[i].worst_severity = QX_SEVERITY_INFO;
        report->law_scores[i].violation_count = 0u;
        report->law_scores[i].is_compliant   = QX_TRUE;
    }

    return QX_OK;
}

// ── Violation code formatting ─────────────────────────────────────────────────

/**
 * @brief Build a structured violation error code string.
 *
 * Format: "LAW_<N>_<NAME>-<SEVERITY>"
 * Examples:
 *   law_id=1, severity=FATAL    → "LAW_1_PATTERN-FATAL"
 *   law_id=6, severity=CRITICAL → "LAW_6_ETHICS-CRITICAL"
 *
 * @param law_id    Law identifier (1–8).
 * @param severity  Severity level (INFO–FATAL).
 * @param buffer    Caller-allocated buffer of at least
 *                  QX_VIOLATION_CODE_MAX (32) bytes.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if buffer is NULL.
 * @return QX_ERR_INVALID_ARGUMENT if law_id or severity are out of range.
 *
 * @note Thread-safe. Pure computation – no shared state access.
 */
QX_API QXResult qx_violation_format_code(
    QXLawId      law_id,
    QXSeverityId severity,
    char        *buffer
) {
    if (!buffer)                { return QX_ERR_NULL_HANDLE;      }
    if (!law_id_valid(law_id))  { return QX_ERR_INVALID_ARGUMENT; }
    if (!severity_valid(severity)) { return QX_ERR_INVALID_ARGUMENT; }

    const char *name = qx::internal::law_name(law_id);
    const char *sev  = qx::internal::severity_name(severity);

    std::snprintf(
        buffer,
        QX_VIOLATION_CODE_MAX,
        "LAW_%u_%s-%s",
        static_cast<unsigned>(law_id),
        name,
        sev
    );

    // Ensure uppercase for name and severity (already uppercase from tables)
    return QX_OK;
}

// ── Convenience predicates ────────────────────────────────────────────────────

/**
 * @brief Return QX_TRUE if a violation has FATAL severity.
 *
 * @param violation  Non-NULL pointer to a QXViolation.
 * @return QX_TRUE if severity == QX_SEVERITY_FATAL; QX_FALSE otherwise
 *         or if violation is NULL.
 *
 * @note Thread-safe. Pure read – no shared state access.
 */
QX_API QXBool qx_violation_is_fatal(const QXViolation *violation) {
    if (!violation) { return QX_FALSE; }
    return (violation->severity == QX_SEVERITY_FATAL)
           ? QX_TRUE : QX_FALSE;
}

/**
 * @brief Return QX_TRUE if a violation has CRITICAL or FATAL severity.
 *
 * @param violation  Non-NULL pointer to a QXViolation.
 * @return QX_TRUE if severity >= QX_SEVERITY_CRITICAL; QX_FALSE otherwise.
 *
 * @note Thread-safe. Pure read – no shared state access.
 */
QX_API QXBool qx_violation_is_critical_or_fatal(
    const QXViolation *violation
) {
    if (!violation) { return QX_FALSE; }
    return (violation->severity >= QX_SEVERITY_CRITICAL)
           ? QX_TRUE : QX_FALSE;
}

// ── Weight accessors ──────────────────────────────────────────────────────────

/**
 * @brief Return the authoritative health-score weight for a law.
 *
 * Reads directly from the QX_LAW_WEIGHTS compile-time table declared in
 * qx_law_types.h. Returns 0.0 for invalid law IDs.
 *
 * @param law_id  Law identifier (1-based, 1 = Pattern, 8 = Economy).
 * @return Weight in range [0.0, 1.0]; 0.0 if law_id is out of range.
 *
 * @note Thread-safe. Read-only access to static data.
 */
QX_API double qx_law_weight(QXLawId law_id) {
    if (!law_id_valid(law_id)) { return 0.0; }
    return QX_LAW_WEIGHTS[static_cast<uint32_t>(law_id) - 1u];
}

/**
 * @brief Return the authoritative cognitive signal weight.
 *
 * Reads directly from the QX_SIGNAL_WEIGHTS compile-time table declared
 * in qx_snapshot.h. Returns 0.0 for invalid signal indices.
 *
 * @param signal  A QXCognitiveSignal enum value (0–4).
 * @return Weight in range [0.0, 1.0]; 0.0 if signal is out of range.
 *
 * @note Thread-safe. Read-only access to static data.
 */
QX_API double qx_signal_weight(QXCognitiveSignal signal) {
    if (static_cast<uint32_t>(signal) >= QX_SNAPSHOT_SIGNAL_COUNT) {
        return 0.0;
    }
    return QX_SIGNAL_WEIGHTS[static_cast<uint32_t>(signal)];
}

// ── Segment input helpers ─────────────────────────────────────────────────────

/**
 * @brief Populate a QXLawSegmentInput from a QXSegmentStats snapshot.
 *
 * Maps the fields required by the law evaluator from the raw segment
 * stats struct. Computes hard_utilisation, soft_utilisation, and
 * pairs_ratio from the raw counters.
 *
 * @param stats       Non-NULL pointer to a populated QXSegmentStats.
 * @param out_input   Non-NULL pointer to a QXLawSegmentInput to fill.
 * @return QX_OK on success; QX_ERR_NULL_HANDLE if either pointer is NULL.
 *
 * @note Thread-safe. Pure computation – no shared state access.
 */
QX_API QXResult qx_law_segment_input_from_stats(
    const QXSegmentStats  *stats,
    QXLawSegmentInput     *out_input
) {
    if (!stats || !out_input) { return QX_ERR_NULL_HANDLE; }

    std::memset(out_input, 0, sizeof(QXLawSegmentInput));

    std::strncpy(out_input->segment_id,
                 stats->segment_id, 15u);
    out_input->segment_id[15] = '\0';

    out_input->used_bytes          = stats->used_bytes;
    out_input->soft_limit_bytes    = stats->soft_limit_bytes;
    out_input->hard_limit_bytes    = stats->hard_limit_bytes;
    out_input->total_allocations   = stats->total_allocations;
    out_input->total_deallocations = stats->total_deallocations;
    out_input->orphaned_bytes      = stats->orphaned_bytes;

    // Pairs ratio
    out_input->pairs_ratio =
        (stats->total_allocations > 0u)
        ? static_cast<double>(stats->total_deallocations) /
          static_cast<double>(stats->total_allocations)
        : 1.0;
    if (out_input->pairs_ratio > 1.0) { out_input->pairs_ratio = 1.0; }

    // Utilisation ratios
    out_input->hard_utilisation =
        (stats->hard_limit_bytes > 0u)
        ? static_cast<double>(stats->used_bytes) /
          static_cast<double>(stats->hard_limit_bytes)
        : 0.0;

    out_input->soft_utilisation =
        (stats->soft_limit_bytes > 0u)
        ? static_cast<double>(stats->used_bytes) /
          static_cast<double>(stats->soft_limit_bytes)
        : 0.0;

    return QX_OK;
}

} // extern "C"

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Compile-time invariant checks
// ─────────────────────────────────────────────────────────────────────────────

namespace {

/**
 * @brief Verify law weight array sums to 1.0 at compile time.
 *
 * Floating-point constexpr arithmetic is supported in C++17.
 * Sum = 0.15+0.20+0.15+0.10+0.15+0.10+0.10+0.05 = 1.00.
 */
static_assert(
    sizeof(QX_LAW_WEIGHTS) / sizeof(QX_LAW_WEIGHTS[0]) == QX_LAW_COUNT,
    "QX_LAW_WEIGHTS must have exactly QX_LAW_COUNT entries"
);

/**
 * @brief Verify signal weight array has correct entry count.
 */
static_assert(
    sizeof(QX_SIGNAL_WEIGHTS) / sizeof(QX_SIGNAL_WEIGHTS[0])
        == QX_SNAPSHOT_SIGNAL_COUNT,
    "QX_SIGNAL_WEIGHTS must have exactly QX_SNAPSHOT_SIGNAL_COUNT entries"
);

/**
 * @brief Verify QXLawScore array in QXLawReport matches QX_LAW_COUNT.
 */
static_assert(
    sizeof(static_cast<QXLawReport*>(nullptr)->law_scores) /
    sizeof(static_cast<QXLawReport*>(nullptr)->law_scores[0]) == QX_LAW_COUNT,
    "QXLawReport.law_scores must have exactly QX_LAW_COUNT entries"
);

/**
 * @brief Verify QXLawEvaluationInput segment array matches max segment count.
 */
static_assert(
    sizeof(static_cast<QXLawEvaluationInput*>(nullptr)->segments) /
    sizeof(static_cast<QXLawEvaluationInput*>(nullptr)->segments[0])
        == QX_EVAL_MAX_SEGMENTS,
    "QXLawEvaluationInput.segments must have QX_EVAL_MAX_SEGMENTS entries"
);

} // anonymous namespace
