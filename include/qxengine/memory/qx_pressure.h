// =============================================================================
// QXEngine Core – include/qxengine/memory/qx_pressure.h
// Owner: Masa Bayu
// Created: 2026-05-24
// Description: C ABI for the QXPressureCoordinator – the four-tier memory
//              pressure management system. Coordinates tiered eviction across
//              all nine QLM segments, consumes OS-level memory signals, records
//              pressure events, and maintains a bounded event history (max 500).
//
// Pressure Tiers:
//   Tier 1 – Normal    : utilisation < 70 %  – no eviction
//   Tier 2 – Elevated  : utilisation ≥ 70 %  – evict class D leaves
//   Tier 3 – High      : utilisation ≥ 85 %  – evict class C + D leaves
//   Tier 4 – Critical  : utilisation ≥ 95 %  – evict class B + C + D leaves
//              (class A leaves are NEVER evicted at any tier)
//
// Ownership:
//   QXPressureCoordinatorHandle is created by qx_pressure_create() and must
//   be destroyed exactly once via qx_pressure_destroy(). It does NOT own the
//   QXMemlocHandle passed to it – the caller is responsible for the memloc
//   lifetime outliving the coordinator.
//
// Thread Safety:
//   All functions except qx_pressure_create() and qx_pressure_destroy() are
//   thread-safe. Create and destroy must be called from the same thread and
//   must not be called concurrently with any other function on this handle.
//
// Repository: https://github.com/qxengine/qxengine-core
// =============================================================================

#ifndef QXENGINE_MEMORY_QX_PRESSURE_H
#define QXENGINE_MEMORY_QX_PRESSURE_H

#include "qxengine/core/qx_types.h"
#include "qxengine/core/qx_error.h"
#include "qxengine/memory/qx_memloc.h"

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Constants
// ─────────────────────────────────────────────────────────────────────────────

/** Maximum number of pressure events retained in history. */
#define QX_PRESSURE_EVENT_HISTORY_CAP   500u

/** Maximum number of leaf IDs captured per pressure event. */
#define QX_PRESSURE_EVENT_MAX_IDS       64u

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXPressureEvent
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Snapshot of a single pressure response cycle.
 *
 * Captured at the end of every qx_pressure_evaluate() call that results in
 * at least one eviction. Events are stored in a ring buffer capped at
 * QX_PRESSURE_EVENT_HISTORY_CAP.
 */
typedef struct QXPressureEvent {
    /** Pressure tier that triggered this event (2, 3, or 4). */
    QXTierId    trigger_tier;

    /** Total resident bytes across all segments immediately before eviction. */
    QXSize      bytes_before;

    /** Total resident bytes across all segments immediately after eviction. */
    QXSize      bytes_after;

    /** Number of bytes successfully reclaimed (bytes_before - bytes_after). */
    QXSize      bytes_reclaimed;

    /** Number of leaves evicted in this cycle. */
    uint32_t    evicted_count;

    /**
     * @brief Null-terminated UUIDs of evicted leaves (up to
     *        QX_PRESSURE_EVENT_MAX_IDS entries).
     *
     * Each entry is a 37-byte buffer (36 UUID chars + null terminator).
     * Entries beyond evicted_count are zero-filled.
     */
    char        evicted_ids[QX_PRESSURE_EVENT_MAX_IDS][37];

    /** Wall-clock timestamp when the event was recorded (ms since epoch). */
    QXTimestamp timestamp_ms;

    /**
     * @brief Human-readable summary of the eviction round.
     *
     * Format: "Tier N eviction – M leaves reclaimed, X bytes freed"
     * Null-terminated, max 128 bytes.
     */
    char        summary[128];
} QXPressureEvent;

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXPressureStats
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Aggregate statistics for the lifetime of the coordinator.
 */
typedef struct QXPressureStats {
    /** Current composite pressure tier across all nine segments. */
    QXTierId    current_tier;

    /** Highest tier ever reached since coordinator creation. */
    QXTierId    peak_tier;

    /** Total number of qx_pressure_evaluate() calls. */
    uint64_t    total_evaluations;

    /** Number of evaluations that resulted in at least one eviction. */
    uint64_t    eviction_cycles;

    /** Cumulative bytes reclaimed across all eviction cycles. */
    QXSize      total_bytes_reclaimed;

    /** Total number of individual leaves evicted since creation. */
    uint64_t    total_leaves_evicted;

    /** Number of pressure events currently stored in history. */
    uint32_t    event_history_count;

    /** Timestamp of the most recent evaluation (ms since epoch). */
    QXTimestamp last_evaluated_ms;

    /** Timestamp of the most recent tier-2+ event (0 if none). */
    QXTimestamp last_pressure_event_ms;
} QXPressureStats;

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Create a new QXPressureCoordinator bound to a QXMemloc.
 *
 * The coordinator does NOT take ownership of @p memloc. The caller must
 * ensure the memloc handle remains valid for the entire lifetime of the
 * returned coordinator handle.
 *
 * @param memloc        Non-NULL handle to an initialised QXMemloc.
 * @param out_pressure  On success, receives the new coordinator handle.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if memloc is NULL.
 * @return QX_ERR_INVALID_ARGUMENT if out_pressure is NULL.
 * @return QX_ERR_INTERNAL on unexpected allocation failure.
 *
 * @note Not thread-safe. Call from the owning thread only.
 */
QX_API QXResult qx_pressure_create(
    QXMemlocHandle                   memloc,
    QXPressureCoordinatorHandle     *out_pressure
);

/**
 * @brief Destroy a QXPressureCoordinator and release all internal resources.
 *
 * The bound QXMemloc is NOT destroyed. After this call the handle is
 * invalid and must not be used.
 *
 * @param pressure  Handle returned by qx_pressure_create(). No-op if NULL.
 *
 * @note Not thread-safe. Call from the owning thread only, after all other
 *       callers have finished using this handle.
 */
QX_API void qx_pressure_destroy(
    QXPressureCoordinatorHandle pressure
);

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Evaluation
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Evaluate current memory pressure and execute evictions if required.
 *
 * This is the primary entry point for the pressure loop. The coordinator:
 *  1. Reads the composite pressure tier from all nine segments.
 *  2. If tier == 1, returns QX_OK immediately (no evictions).
 *  3. For tiers 2-4, selects eviction candidates respecting leaf-class rules:
 *       Tier 2 → evict class D
 *       Tier 3 → evict class D, then C if tier 3 still active
 *       Tier 4 → evict class D, then C, then B if tier 4 still active
 *       Class A is NEVER a candidate regardless of tier.
 *  4. Executes evictions via qx_memloc_* and records a QXPressureEvent.
 *  5. Re-checks the tier after eviction; records whether relief was achieved.
 *
 * @param pressure      Non-NULL coordinator handle.
 * @param out_event     Optional. If non-NULL and an eviction cycle occurred,
 *                      the event snapshot is written here. No-op if tier 1.
 * @return QX_OK on success (tier 1 or successful eviction).
 * @return QX_ERR_NULL_HANDLE if pressure is NULL.
 * @return QX_ERR_INSUFFICIENT_EVICTION if tier 4 and no relief achieved.
 * @return QX_ERR_INTERNAL on unexpected failure.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_pressure_evaluate(
    QXPressureCoordinatorHandle  pressure,
    QXPressureEvent             *out_event
);

/**
 * @brief Evaluate and evict across ALL segments simultaneously.
 *
 * Like qx_pressure_evaluate() but ignores per-segment pressure tiers and
 * forces a global pass at the specified tier. Used when an OS-level signal
 * (e.g., Android onTrimMemory COMPLETE, iOS memory warning) demands
 * immediate, engine-wide relief.
 *
 * @param pressure      Non-NULL coordinator handle.
 * @param forced_tier   Tier to enforce (must be 2, 3, or 4).
 * @param out_event     Optional. Receives the eviction event if non-NULL.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if pressure is NULL.
 * @return QX_ERR_INVALID_ARGUMENT if forced_tier is not 2, 3, or 4.
 * @return QX_ERR_INSUFFICIENT_EVICTION if forced tier 4 and no relief.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_pressure_evaluate_all(
    QXPressureCoordinatorHandle  pressure,
    QXTierId                     forced_tier,
    QXPressureEvent             *out_event
);

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – OS Memory Signal Integration
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Map an Android onTrimMemory level to a pressure tier and evaluate.
 *
 * Android trim levels are mapped as follows:
 *
 *   TRIM_MEMORY_RUNNING_MODERATE (5)  → Tier 2
 *   TRIM_MEMORY_RUNNING_LOW      (10) → Tier 3
 *   TRIM_MEMORY_RUNNING_CRITICAL (15) → Tier 4
 *   TRIM_MEMORY_UI_HIDDEN        (20) → Tier 2
 *   TRIM_MEMORY_BACKGROUND       (40) → Tier 2
 *   TRIM_MEMORY_MODERATE         (60) → Tier 3
 *   TRIM_MEMORY_COMPLETE         (80) → Tier 4
 *   All other values                  → Tier 2 (conservative default)
 *
 * @param pressure          Non-NULL coordinator handle.
 * @param android_trim_level  Integer level received from onTrimMemory().
 * @param out_event         Optional. Receives the event snapshot if non-NULL.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if pressure is NULL.
 * @return QX_ERR_INSUFFICIENT_EVICTION if tier 4 and no relief achieved.
 *
 * @note Thread-safe. Intended for use in the Android platform adapter only.
 */
QX_API QXResult qx_pressure_handle_android_trim(
    QXPressureCoordinatorHandle  pressure,
    int32_t                      android_trim_level,
    QXPressureEvent             *out_event
);

/**
 * @brief Handle an iOS didReceiveMemoryWarning or UIApplication memory event.
 *
 * iOS does not provide a numeric level; all memory warnings are treated as
 * a Tier 3 event by default. If the process's resident memory exceeds 80 %
 * of the device physical RAM (as fed via qx_pressure_feed_memory()), the
 * coordinator escalates to Tier 4 automatically.
 *
 * @param pressure      Non-NULL coordinator handle.
 * @param out_event     Optional. Receives the event snapshot if non-NULL.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if pressure is NULL.
 * @return QX_ERR_INSUFFICIENT_EVICTION if tier 4 and no relief achieved.
 *
 * @note Thread-safe. Intended for use in the iOS platform adapter only.
 */
QX_API QXResult qx_pressure_handle_ios_warning(
    QXPressureCoordinatorHandle  pressure,
    QXPressureEvent             *out_event
);

/**
 * @brief Feed current OS memory readings into the coordinator.
 *
 * Called by the platform adapter (QXMemoryReader) to supply real-time
 * memory telemetry. The coordinator uses these values to:
 *  - Compute system-wide utilisation for tier classification.
 *  - Auto-escalate tier during qx_pressure_handle_ios_warning().
 *  - Record accurate bytes_before / bytes_after in QXPressureEvent.
 *
 * @param pressure          Non-NULL coordinator handle.
 * @param resident_bytes    Process resident bytes (PSS on Android, RSS on iOS).
 * @param system_total_bytes  Total physical device RAM in bytes.
 * @param system_avail_bytes  Currently available RAM in bytes.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if pressure is NULL.
 * @return QX_ERR_INVALID_ARGUMENT if system_total_bytes == 0.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_pressure_feed_memory(
    QXPressureCoordinatorHandle  pressure,
    QXSize                       resident_bytes,
    QXSize                       system_total_bytes,
    QXSize                       system_avail_bytes
);

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Tier Query
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Return the current composite pressure tier without triggering eviction.
 *
 * The composite tier is computed as the maximum tier across all nine segments.
 * Segment tiers are derived from their hard-limit utilisation percentage:
 *
 *   used / hard_limit < 0.70  → Tier 1
 *   used / hard_limit ≥ 0.70  → Tier 2
 *   used / hard_limit ≥ 0.85  → Tier 3
 *   used / hard_limit ≥ 0.95  → Tier 4
 *
 * @param pressure   Non-NULL coordinator handle.
 * @param out_tier   Receives the composite tier (1–4).
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if pressure or out_tier is NULL.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_pressure_current_tier(
    QXPressureCoordinatorHandle  pressure,
    QXTierId                    *out_tier
);

/**
 * @brief Return the pressure tier for a specific segment.
 *
 * @param pressure    Non-NULL coordinator handle.
 * @param segment_id  Null-terminated segment ID string (max 15 chars).
 * @param out_tier    Receives the segment tier (1–4).
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if pressure or out_tier is NULL.
 * @return QX_ERR_UNKNOWN_SEGMENT if segment_id is not in the manifest.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_pressure_segment_tier(
    QXPressureCoordinatorHandle  pressure,
    const char                  *segment_id,
    QXTierId                    *out_tier
);

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Event History
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Return the number of events currently stored in the event history.
 *
 * @param pressure   Non-NULL coordinator handle.
 * @param out_count  Receives the event count (0 – QX_PRESSURE_EVENT_HISTORY_CAP).
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if pressure or out_count is NULL.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_pressure_event_count(
    QXPressureCoordinatorHandle  pressure,
    uint32_t                    *out_count
);

/**
 * @brief Retrieve a pressure event by index (0 = oldest, count-1 = newest).
 *
 * @param pressure   Non-NULL coordinator handle.
 * @param index      Zero-based index into the event history.
 * @param out_event  Receives a copy of the event at @p index.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if pressure or out_event is NULL.
 * @return QX_ERR_INVALID_ARGUMENT if index ≥ event count.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_pressure_event_at(
    QXPressureCoordinatorHandle  pressure,
    uint32_t                     index,
    QXPressureEvent             *out_event
);

/**
 * @brief Copy up to @p capacity events from the history into @p out_events.
 *
 * Events are ordered oldest-first. This is a bulk read; useful for telemetry
 * batch export.
 *
 * @param pressure      Non-NULL coordinator handle.
 * @param out_events    Caller-allocated array of QXPressureEvent.
 * @param capacity      Number of elements in @p out_events.
 * @param out_written   Receives the number of events actually written.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if any pointer is NULL.
 * @return QX_ERR_INVALID_ARGUMENT if capacity == 0.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_pressure_events_copy(
    QXPressureCoordinatorHandle  pressure,
    QXPressureEvent             *out_events,
    uint32_t                     capacity,
    uint32_t                    *out_written
);

/**
 * @brief Clear all events from the pressure event history.
 *
 * @param pressure  Non-NULL coordinator handle.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if pressure is NULL.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_pressure_clear_history(
    QXPressureCoordinatorHandle pressure
);

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Statistics
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Return aggregate lifetime statistics for the coordinator.
 *
 * @param pressure   Non-NULL coordinator handle.
 * @param out_stats  Receives a snapshot of the coordinator's aggregate stats.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if pressure or out_stats is NULL.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_pressure_stats(
    QXPressureCoordinatorHandle  pressure,
    QXPressureStats             *out_stats
);

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Eviction-Class Eligibility Matrix
// ─────────────────────────────────────────────────────────────────────────────
//
//  ┌────────────┬────────┬────────┬────────┬────────┐
//  │ Leaf Class │ Tier 1 │ Tier 2 │ Tier 3 │ Tier 4 │
//  ├────────────┼────────┼────────┼────────┼────────┤
//  │ A Protected│   ✗    │   ✗    │   ✗    │   ✗    │
//  │ B Essential│   ✗    │   ✗    │   ✗    │   ✓    │
//  │ C Standard │   ✗    │   ✗    │   ✓    │   ✓    │
//  │ D Transient│   ✗    │   ✓    │   ✓    │   ✓    │
//  └────────────┴────────┴────────┴────────┴────────┘
//
// This matrix is authoritative. Any deviation is a constitutional violation
// of Law 2 (Limit) and will be flagged by the QXLawEnforcer.
// ─────────────────────────────────────────────────────────────────────────────

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* QXENGINE_MEMORY_QX_PRESSURE_H */
