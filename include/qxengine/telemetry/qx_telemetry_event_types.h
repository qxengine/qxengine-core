// =============================================================================
// QXEngine Core – include/qxengine/telemetry/qx_telemetry_event_types.h
// Owner: Masa Bayu
// Created: 2026-05-24
// Description: Telemetry event types – category, tag, event record, and input.
//              Included by qx_telemetry_types.h.
//
// Event Categories:
//   QX_TEL_ALLOCATION     – leaf allocation (success or failure).
//   QX_TEL_DEALLOCATION   – leaf deallocation (success or failure).
//   QX_TEL_EVICTION       – leaf eviction triggered by pressure.
//   QX_TEL_PRESSURE       – pressure tier change event.
//   QX_TEL_EVALUATION     – constitutional law evaluation cycle.
//   QX_TEL_SNAPSHOT       – cognitive snapshot captured.
//   QX_TEL_ENGINE         – engine lifecycle event (start, pause, stop).
//   QX_TEL_VIOLATION      – constitutional law violation detected.
//   QX_TEL_SECURITY       – security or integrity event.
//   QX_TEL_NATIVE         – native capability registration or change.
//
// Repository: https://github.com/qxengine/qxengine-core
// =============================================================================

#ifndef QXENGINE_TELEMETRY_QX_TELEMETRY_EVENT_TYPES_H
#define QXENGINE_TELEMETRY_QX_TELEMETRY_EVENT_TYPES_H

#include "qxengine/core/qx_types.h"
#include "qxengine/core/qx_error.h"
#include "qxengine/law/qx_law_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Event Constants
// ─────────────────────────────────────────────────────────────────────────────

/** Maximum number of telemetry events retained in the event log. */
#define QX_TEL_EVENT_CAP            1000u

/** Maximum length of an event message string. */
#define QX_TEL_MESSAGE_MAX          256u

/** Maximum length of an event detail string. */
#define QX_TEL_DETAIL_MAX           128u

/** Maximum length of a leaf label within a telemetry event. */
#define QX_TEL_LABEL_MAX            129u

/** Maximum length of a segment ID within a telemetry event. */
#define QX_TEL_SEGMENT_ID_MAX       16u

/** Maximum length of a leaf UUID within a telemetry event. */
#define QX_TEL_LEAF_ID_MAX          37u

/** Maximum number of tag key-value pairs per telemetry event. */
#define QX_TEL_MAX_TAGS             8u

/** Maximum length of a tag key or value string. */
#define QX_TEL_TAG_MAX              64u

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXTelemetryCategory
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Category of a telemetry event.
 *
 * Used for filtering, aggregation, and routing to external sinks.
 */
typedef enum QXTelemetryCategory {
    QX_TEL_ALLOCATION   = 0,    /**< Leaf allocation attempt (success/fail). */
    QX_TEL_DEALLOCATION = 1,    /**< Leaf deallocation attempt.              */
    QX_TEL_EVICTION     = 2,    /**< Leaf eviction by pressure coordinator.  */
    QX_TEL_PRESSURE     = 3,    /**< Pressure tier change detected.          */
    QX_TEL_EVALUATION   = 4,    /**< Law enforcement evaluation cycle.       */
    QX_TEL_SNAPSHOT     = 5,    /**< Cognitive snapshot captured.            */
    QX_TEL_ENGINE       = 6,    /**< Engine lifecycle transition.            */
    QX_TEL_VIOLATION    = 7,    /**< Constitutional law violation detected.  */
    QX_TEL_SECURITY     = 8,    /**< Security or integrity event.            */
    QX_TEL_NATIVE       = 9     /**< Native capability registration/change.  */
} QXTelemetryCategory;

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXTelemetryTag
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief A key-value metadata tag attached to a telemetry event.
 *
 * Tags enable flexible filtering and enrichment without changing the core
 * event schema. Both key and value are null-terminated UTF-8 strings.
 */
typedef struct QXTelemetryTag {
    char key[QX_TEL_TAG_MAX];   /**< Tag key.   Max QX_TEL_TAG_MAX bytes. */
    char value[QX_TEL_TAG_MAX]; /**< Tag value. Max QX_TEL_TAG_MAX bytes. */
} QXTelemetryTag;

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXTelemetryEvent
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief A single structured telemetry event.
 *
 * Every significant engine operation produces one event. Events are stored
 * in a ring buffer capped at QX_TEL_EVENT_CAP and can be exported in bulk.
 */
typedef struct QXTelemetryEvent {
    // ── Identity ─────────────────────────────────────────────────────────────

    /**
     * @brief Monotonically increasing sequence number.
     *
     * Starts at 1 on the first event. Never resets within a telemetry
     * handle lifetime. Used for ordering and deduplication.
     */
    QXSequence          sequence;

    /** Category of this event. */
    QXTelemetryCategory category;

    /** Wall-clock timestamp when this event was recorded (ms since epoch). */
    QXTimestamp         timestamp_ms;

    // ── Result ───────────────────────────────────────────────────────────────

    /**
     * @brief Result code of the operation that produced this event.
     *
     * QX_OK (0) on success; negative QX_ERR_* on failure.
     */
    QXResult            result;

    /**
     * @brief QX_TRUE if the operation succeeded (result == QX_OK).
     */
    QXBool              is_success;

    // ── Message ───────────────────────────────────────────────────────────────

    /**
     * @brief Human-readable event summary.
     *
     * Format depends on category:
     *   ALLOCATION   – "Allocated 'label' (X MB) in QLM-SEG [slot N]"
     *   DEALLOCATION – "Deallocated 'label' from QLM-SEG (X MB freed)"
     *   EVICTION     – "Evicted 'label' from QLM-SEG (tier N)"
     *   PRESSURE     – "Pressure tier N→M in QLM-SEG"
     *   EVALUATION   – "Law evaluation #N – score X.X – TIER"
     *   SNAPSHOT     – "Cognitive snapshot #N – score X.X"
     *   ENGINE       – "Engine STATE_FROM → STATE_TO"
     *   VIOLATION    – "LAW_N_NAME-SEVERITY: message"
     *   SECURITY     – "Security event: description"
     *   NATIVE       – "Native capability 'id' ACTIVE|INACTIVE"
     * Null-terminated. Max QX_TEL_MESSAGE_MAX bytes.
     */
    char                message[QX_TEL_MESSAGE_MAX];

    /**
     * @brief Optional structured detail string.
     *
     * Contains machine-parseable context such as raw byte counts, segment
     * IDs, or error reasons. Null-terminated. Max QX_TEL_DETAIL_MAX bytes.
     * Empty string when not applicable.
     */
    char                detail[QX_TEL_DETAIL_MAX];

    // ── Memory Context ────────────────────────────────────────────────────────

    /**
     * @brief Leaf UUID involved in this event (if applicable).
     *
     * Non-empty for ALLOCATION, DEALLOCATION, EVICTION events.
     * Empty string otherwise. Null-terminated. Max QX_TEL_LEAF_ID_MAX bytes.
     */
    char                leaf_id[QX_TEL_LEAF_ID_MAX];

    /**
     * @brief Leaf label involved in this event (if applicable).
     *
     * Non-empty for ALLOCATION, DEALLOCATION, EVICTION events.
     * Null-terminated. Max QX_TEL_LABEL_MAX bytes.
     */
    char                leaf_label[QX_TEL_LABEL_MAX];

    /**
     * @brief Segment ID involved in this event (if applicable).
     *
     * Non-empty for ALLOCATION, DEALLOCATION, EVICTION, PRESSURE events.
     * Null-terminated. Max QX_TEL_SEGMENT_ID_MAX bytes.
     */
    char                segment_id[QX_TEL_SEGMENT_ID_MAX];

    /**
     * @brief Bytes involved in this event (if applicable).
     *
     * Allocation size for ALLOCATION; bytes freed for DEALLOCATION;
     * bytes reclaimed for EVICTION; 0 otherwise.
     */
    QXSize              bytes;

    // ── Law / Evaluation Context ──────────────────────────────────────────────

    /**
     * @brief Health score from an EVALUATION event (0.0 – 100.0).
     *
     * Non-zero only for QX_TEL_EVALUATION and QX_TEL_SNAPSHOT events.
     */
    double              health_score;

    /**
     * @brief Pressure tier for PRESSURE and EVICTION events (1–4).
     *
     * Zero for all other categories.
     */
    QXTierId            pressure_tier;

    /**
     * @brief Law ID for VIOLATION events.
     *
     * Zero (invalid) for all other categories.
     */
    QXLawId             law_id;

    /**
     * @brief Severity for VIOLATION events.
     *
     * Zero (INFO) for all other categories.
     */
    QXSeverityId        severity;

    // ── Tags ─────────────────────────────────────────────────────────────────

    /**
     * @brief Optional metadata tags for this event.
     *
     * Up to QX_TEL_MAX_TAGS (8) key-value pairs.
     * Valid entries: tag_count.
     */
    QXTelemetryTag      tags[QX_TEL_MAX_TAGS];

    /** Number of valid entries in @p tags. */
    uint32_t            tag_count;

} QXTelemetryEvent;

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXTelemetryEventInput
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Input for recording a telemetry event via qx_telemetry_record().
 *
 * Callers populate only the fields relevant to their event category.
 * The telemetry engine fills sequence, timestamp_ms, is_success, and
 * message automatically if they are left at zero / empty.
 */
typedef struct QXTelemetryEventInput {
    QXTelemetryCategory category;
    QXResult            result;
    char                message[QX_TEL_MESSAGE_MAX];
    char                detail[QX_TEL_DETAIL_MAX];
    char                leaf_id[QX_TEL_LEAF_ID_MAX];
    char                leaf_label[QX_TEL_LABEL_MAX];
    char                segment_id[QX_TEL_SEGMENT_ID_MAX];
    QXSize              bytes;
    double              health_score;
    QXTierId            pressure_tier;
    QXLawId             law_id;
    QXSeverityId        severity;
    QXTelemetryTag      tags[QX_TEL_MAX_TAGS];
    uint32_t            tag_count;
} QXTelemetryEventInput;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* QXENGINE_TELEMETRY_QX_TELEMETRY_EVENT_TYPES_H */
