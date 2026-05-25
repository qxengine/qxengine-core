// =============================================================================
// QXEngine Core – include/qxengine/telemetry/qx_telemetry.h
// Owner: Masa Bayu
// Created: 2026-05-24
// Description: C ABI for the QXTelemetry subsystem – lifecycle, recording,
//              log access, reporting, and statistics. Types are in
//              qx_telemetry_types.h.
//
// Ownership:
//   QXTelemetryHandle is created by qx_telemetry_create() and must be
//   destroyed exactly once via qx_telemetry_destroy(). The handle does
//   NOT own any subsystem handles passed to telemetry functions; the
//   caller is responsible for the lifetime of those handles.
//
// Thread Safety:
//   qx_telemetry_create() and qx_telemetry_destroy() are NOT thread-safe.
//   All record and query functions are thread-safe.
//
// Repository: https://github.com/qxengine/qxengine-core
// =============================================================================

#ifndef QXENGINE_TELEMETRY_QX_TELEMETRY_H
#define QXENGINE_TELEMETRY_QX_TELEMETRY_H

#include "qxengine/telemetry/qx_telemetry_types.h"
#include "qxengine/law/qx_law_report.h"

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Create a new QXTelemetry instance.
 *
 * Allocates two ring buffers: the event log (QX_TEL_EVENT_CAP entries)
 * and the violation log (QX_TEL_VIOLATION_CAP entries).
 *
 * @param config         Optional configuration. Pass NULL to use all defaults
 *                       (all recording flags QX_TRUE, verbose QX_FALSE).
 * @param out_telemetry  On success, receives the new telemetry handle.
 * @return QX_OK on success.
 * @return QX_ERR_INVALID_ARGUMENT if out_telemetry is NULL.
 * @return QX_ERR_INTERNAL on memory allocation failure.
 *
 * @note Not thread-safe.
 */
QX_API QXResult qx_telemetry_create(
    const QXTelemetryConfig *config,
    QXTelemetryHandle       *out_telemetry
);

/**
 * @brief Destroy a QXTelemetry instance and release all internal resources.
 *
 * All buffered events and violations are discarded. After this call the
 * handle is invalid and must not be used.
 *
 * @param telemetry  Handle returned by qx_telemetry_create(). No-op if NULL.
 *
 * @note Not thread-safe. Ensure no concurrent calls are in progress.
 */
QX_API void qx_telemetry_destroy(
    QXTelemetryHandle telemetry
);

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Event Recording
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Record a telemetry event.
 *
 * The event is appended to the ring buffer. If the buffer is full the
 * oldest event is overwritten. The sequence number is assigned atomically.
 *
 * If telemetry is disabled (QXTelemetryConfig.enabled == QX_FALSE) this
 * function returns QX_OK immediately without recording anything.
 *
 * If the event category is QX_TEL_VIOLATION and
 * QXTelemetryConfig.separate_violation_log == QX_TRUE the event is also
 * appended to the dedicated violation log.
 *
 * @param telemetry  Non-NULL telemetry handle.
 * @param input      Non-NULL pointer to a populated QXTelemetryEventInput.
 * @param out_event  Optional. If non-NULL, receives a copy of the recorded
 *                   event with all auto-filled fields populated.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if telemetry or input is NULL.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_telemetry_record(
    QXTelemetryHandle            telemetry,
    const QXTelemetryEventInput *input,
    QXTelemetryEvent            *out_event
);

/**
 * @brief Record a violation event directly from a QXViolation struct.
 *
 * Convenience wrapper around qx_telemetry_record() that maps a QXViolation
 * (produced by QXLawEnforcer) to a QX_TEL_VIOLATION event automatically.
 *
 * @param telemetry    Non-NULL telemetry handle.
 * @param violation    Non-NULL pointer to a QXViolation.
 * @param out_event    Optional. Receives a copy of the recorded event.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if telemetry or violation is NULL.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_telemetry_record_violation(
    QXTelemetryHandle    telemetry,
    const QXViolation   *violation,
    QXTelemetryEvent    *out_event
);

/**
 * @brief Record all violations from a QXLawReport in a single call.
 *
 * Iterates over report->violations[0..violation_count-1] and calls
 * qx_telemetry_record_violation() for each entry. Also records one
 * QX_TEL_EVALUATION event summarising the full report.
 *
 * @param telemetry    Non-NULL telemetry handle.
 * @param report       Non-NULL pointer to a committed QXLawReport.
 * @param out_recorded Optional. Receives the total number of events recorded.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if telemetry or report is NULL.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_telemetry_record_report(
    QXTelemetryHandle   telemetry,
    const QXLawReport  *report,
    uint32_t           *out_recorded
);

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Event Log Access
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Return the number of events currently stored in the event log.
 *
 * @param telemetry  Non-NULL telemetry handle.
 * @param out_count  Receives the count (0 – QX_TEL_EVENT_CAP).
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if either argument is NULL.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_telemetry_event_count(
    QXTelemetryHandle  telemetry,
    uint32_t          *out_count
);

/**
 * @brief Retrieve a telemetry event by index (0 = oldest, count-1 = newest).
 *
 * @param telemetry  Non-NULL telemetry handle.
 * @param index      Zero-based index into the event log.
 * @param out_event  Non-NULL. Receives a copy of the event at @p index.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if telemetry or out_event is NULL.
 * @return QX_ERR_INVALID_ARGUMENT if index ≥ current event count.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_telemetry_event_at(
    QXTelemetryHandle  telemetry,
    uint32_t           index,
    QXTelemetryEvent  *out_event
);

/**
 * @brief Bulk-copy up to @p capacity events from the log (oldest-first).
 *
 * @param telemetry     Non-NULL telemetry handle.
 * @param out_events    Caller-allocated array of QXTelemetryEvent.
 * @param capacity      Number of elements in @p out_events.
 * @param out_written   Receives the number of events actually written.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if any pointer is NULL.
 * @return QX_ERR_INVALID_ARGUMENT if capacity == 0.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_telemetry_events_copy(
    QXTelemetryHandle  telemetry,
    QXTelemetryEvent  *out_events,
    uint32_t           capacity,
    uint32_t          *out_written
);

/**
 * @brief Filter events by category and copy up to @p capacity matches.
 *
 * Events are returned oldest-first within the matching category.
 *
 * @param telemetry     Non-NULL telemetry handle.
 * @param category      The category to filter by.
 * @param out_events    Caller-allocated array of QXTelemetryEvent.
 * @param capacity      Number of elements in @p out_events.
 * @param out_written   Receives the number of events actually written.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if any pointer is NULL.
 * @return QX_ERR_INVALID_ARGUMENT if capacity == 0.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_telemetry_events_by_category(
    QXTelemetryHandle    telemetry,
    QXTelemetryCategory  category,
    QXTelemetryEvent    *out_events,
    uint32_t             capacity,
    uint32_t            *out_written
);

/**
 * @brief Clear all events from the event log.
 *
 * The sequence counter is NOT reset; the next event receives the next
 * sequence number in line, preserving audit continuity.
 *
 * @param telemetry  Non-NULL telemetry handle.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if telemetry is NULL.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_telemetry_clear_events(
    QXTelemetryHandle telemetry
);

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Violation Log Access
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Return the number of violations currently stored in the violation log.
 *
 * @param telemetry  Non-NULL telemetry handle.
 * @param out_count  Receives the count (0 – QX_TEL_VIOLATION_CAP).
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if either argument is NULL.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_telemetry_violation_count(
    QXTelemetryHandle  telemetry,
    uint32_t          *out_count
);

/**
 * @brief Retrieve a violation record by index (0 = oldest, count-1 = newest).
 *
 * @param telemetry     Non-NULL telemetry handle.
 * @param index         Zero-based index into the violation log.
 * @param out_violation Non-NULL. Receives a copy of the QXViolation.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if telemetry or out_violation is NULL.
 * @return QX_ERR_INVALID_ARGUMENT if index ≥ current violation count.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_telemetry_violation_at(
    QXTelemetryHandle  telemetry,
    uint32_t           index,
    QXViolation       *out_violation
);

/**
 * @brief Bulk-copy up to @p capacity violation records (oldest-first).
 *
 * @param telemetry       Non-NULL telemetry handle.
 * @param out_violations  Caller-allocated array of QXViolation.
 * @param capacity        Number of elements in @p out_violations.
 * @param out_written     Receives the number of violations actually written.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if any pointer is NULL.
 * @return QX_ERR_INVALID_ARGUMENT if capacity == 0.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_telemetry_violations_copy(
    QXTelemetryHandle  telemetry,
    QXViolation       *out_violations,
    uint32_t           capacity,
    uint32_t          *out_written
);

/**
 * @brief Filter violation records by law ID and copy up to @p capacity matches.
 *
 * @param telemetry       Non-NULL telemetry handle.
 * @param law_id          Law to filter by (QX_LAW_PATTERN – QX_LAW_ECONOMY).
 * @param out_violations  Caller-allocated array of QXViolation.
 * @param capacity        Number of elements in @p out_violations.
 * @param out_written     Receives the number of violations actually written.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if any pointer is NULL.
 * @return QX_ERR_INVALID_ARGUMENT if capacity == 0 or law_id out of range.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_telemetry_violations_by_law(
    QXTelemetryHandle  telemetry,
    QXLawId            law_id,
    QXViolation       *out_violations,
    uint32_t           capacity,
    uint32_t          *out_written
);

/**
 * @brief Clear all records from the violation log.
 *
 * Does NOT clear the main event log. Call qx_telemetry_clear_events()
 * separately if a full reset is required.
 *
 * @param telemetry  Non-NULL telemetry handle.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if telemetry is NULL.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_telemetry_clear_violations(
    QXTelemetryHandle telemetry
);

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Reporting
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Generate a structured QXTelemetryReport from current state.
 *
 * Aggregates event counts, violation breakdowns, memory and intelligence
 * summaries, and produces a human-readable multi-line summary string.
 *
 * @param telemetry   Non-NULL telemetry handle.
 * @param health_score Current engine health score (0.0 – 100.0) to embed
 *                    in the report header. Supplied by the caller from the
 *                    most recent QXLawReport.
 * @param out_report  Non-NULL. Receives the generated report.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if telemetry or out_report is NULL.
 * @return QX_ERR_INVALID_ARGUMENT if health_score not in [0.0, 100.0].
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_telemetry_report(
    QXTelemetryHandle   telemetry,
    double              health_score,
    QXTelemetryReport  *out_report
);

/**
 * @brief Serialise the telemetry report to a JSON string.
 *
 * Produces a compact, valid UTF-8 JSON object containing all report fields.
 * Useful for export to external analytics, dashboards, or audit archives.
 *
 * @param report        Non-NULL pointer to a QXTelemetryReport.
 * @param buffer        Caller-allocated output buffer.
 * @param buffer_size   Size of @p buffer in bytes.
 * @param out_written   Optional. Receives bytes written (excluding null).
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if report or buffer is NULL.
 * @return QX_ERR_BUFFER_TOO_SMALL if buffer_size is insufficient.
 *         *out_written receives the required size in this case.
 *
 * @note Thread-safe. Pure serialisation – no internal state is modified.
 */
QX_API QXResult qx_telemetry_report_to_json(
    const QXTelemetryReport *report,
    char                    *buffer,
    uint32_t                 buffer_size,
    uint32_t                *out_written
);

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Statistics
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Return aggregate lifetime statistics for the telemetry instance.
 *
 * @param telemetry  Non-NULL telemetry handle.
 * @param out_stats  Receives a snapshot of the telemetry's aggregate stats.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if telemetry or out_stats is NULL.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_telemetry_stats(
    QXTelemetryHandle  telemetry,
    QXTelemetryStats  *out_stats
);

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Category Label Helper
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Return a human-readable label for a QXTelemetryCategory value.
 *
 * @param category  A QXTelemetryCategory value.
 * @return Null-terminated string literal:
 *         "Allocation", "Deallocation", "Eviction", "Pressure",
 *         "Evaluation", "Snapshot", "Engine", "Violation",
 *         "Security", "Native", or "Unknown".
 *
 * @note Returns a pointer to a string literal; do not free.
 * @note Thread-safe.
 */
QX_API const char *qx_telemetry_category_label(QXTelemetryCategory category);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* QXENGINE_TELEMETRY_QX_TELEMETRY_H */
