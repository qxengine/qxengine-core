// =============================================================================
// QXEngine Core – include/qxengine/intelligence/qx_snapshot.h
// Owner : Masa Bayu
// Created: 2026-05-24
// Repo   : https://github.com/qxengine/qxengine-core
// Description: C ABI for QXSnapshotHistory – the intelligence layer that
//              periodically samples engine state, computes a cognitive
//              knowledge score (0.0 – 100.0), and maintains a bounded rolling
//              history of snapshots (max QX_SNAPSHOT_HISTORY_MAX = 720).
//
// Cognitive Knowledge Score:
//   Weighted composite of five signals (weights sum to 1.0):
//     Memory clarity       0.25   QXMemloc segment stats
//     Pressure stability   0.20   QXPressureCoordinator history
//     Law compliance       0.25   QXLawEnforcer report history
//     Native coverage      0.15   QXNativeRegistry (platform)
//     Snapshot recency     0.15   seconds_since_last_snapshot
//
//   score = Σ (signal_value[i] × weight[i]) × 100.0
//
// Law 5 thresholds:
//   score ≥ 90.0 → COMPLIANT   score ≥ 70.0 → WARNING
//   score ≥ 50.0 → CRITICAL    score <  50.0 → FATAL
//
// Thread Safety:
//   qx_snapshot_history_create() and qx_snapshot_history_destroy() are NOT
//   thread-safe. All other functions are thread-safe.
//
// Repository: https://github.com/qxengine/qxengine-core
// =============================================================================

#ifndef QXENGINE_INTELLIGENCE_QX_SNAPSHOT_H
#define QXENGINE_INTELLIGENCE_QX_SNAPSHOT_H

#include "qxengine/core/qx_error.h"
#include "qxengine/intelligence/qx_snapshot_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Create a new QXSnapshotHistory ring buffer.
 *
 * Allocates a fixed ring buffer of QX_SNAPSHOT_HISTORY_MAX entries.
 * The buffer is zero-initialised; history count starts at 0.
 *
 * @param out_history  On success, receives the new history handle.
 * @return QX_OK on success.
 * @return QX_ERR_INVALID_ARGUMENT if out_history is NULL.
 * @return QX_ERR_INTERNAL on memory allocation failure.
 *
 * @note Not thread-safe. Call from the owning thread only.
 */
QX_API QXResult qx_snapshot_history_create(
    QXSnapshotHistoryHandle *out_history
);

/**
 * @brief Destroy a QXSnapshotHistory and release all internal resources.
 *
 * After this call the handle is invalid and must not be used.
 *
 * @param history  Handle returned by qx_snapshot_history_create(). No-op if NULL.
 *
 * @note Not thread-safe. Ensure no concurrent calls are in progress.
 */
QX_API void qx_snapshot_history_destroy(
    QXSnapshotHistoryHandle history
);

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Capture
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Compute and commit a new QXCognitiveSnapshot to the history.
 *
 * Steps performed internally:
 *   1. Validate @p input (non-NULL, segment_count in range, values in bounds).
 *   2. Compute all five signal values from @p input.
 *   3. Compute weighted_contributions and knowledge_score.
 *   4. Set is_law5_compliant.
 *   5. Assign the next sequence number.
 *   6. Append to the ring buffer (overwrites oldest if full).
 *   7. Optionally write snapshot to *out_snapshot.
 *
 * @param history       Non-NULL history handle.
 * @param input         Non-NULL pointer to a fully populated QXSnapshotInput.
 * @param out_snapshot  Optional. If non-NULL, receives a copy of the snapshot.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if history is NULL.
 * @return QX_ERR_INVALID_ARGUMENT if input is NULL, segment_count == 0,
 *         segment_count > QX_SEGMENT_COUNT, health score out of [0, 100],
 *         or native_coverage_ratio out of [0.0, 1.0].
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_snapshot_capture(
    QXSnapshotHistoryHandle  history,
    const QXSnapshotInput   *input,
    QXCognitiveSnapshot     *out_snapshot
);

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – History Access
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Return the number of snapshots currently stored in the history.
 *
 * @param history    Non-NULL history handle.
 * @param out_count  Receives the count (0 – QX_SNAPSHOT_HISTORY_MAX).
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if history or out_count is NULL.
 */
QX_API QXResult qx_snapshot_count(
    QXSnapshotHistoryHandle  history,
    uint32_t                *out_count
);

/**
 * @brief Retrieve a snapshot by logical index (0 = oldest, count-1 = newest).
 *
 * @param history       Non-NULL history handle.
 * @param index         Zero-based logical index.
 * @param out_snapshot  Non-NULL. Receives a copy of the snapshot.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if history or out_snapshot is NULL.
 * @return QX_ERR_INVALID_ARGUMENT if index ≥ current snapshot count.
 */
QX_API QXResult qx_snapshot_at(
    QXSnapshotHistoryHandle  history,
    uint32_t                 index,
    QXCognitiveSnapshot     *out_snapshot
);

/**
 * @brief Retrieve the most recently captured snapshot.
 *
 * @param history       Non-NULL history handle.
 * @param out_snapshot  Non-NULL. Receives a copy of the latest snapshot.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if history or out_snapshot is NULL.
 * @return QX_ERR_INVALID_ARGUMENT if history is empty.
 */
QX_API QXResult qx_snapshot_latest(
    QXSnapshotHistoryHandle  history,
    QXCognitiveSnapshot     *out_snapshot
);

/**
 * @brief Bulk-copy up to @p capacity snapshots from history, oldest-first.
 *
 * @param history       Non-NULL history handle.
 * @param out_snapshots Caller-allocated array of QXCognitiveSnapshot.
 * @param capacity      Number of elements in @p out_snapshots.
 * @param out_written   Receives the number of snapshots actually written.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if any pointer is NULL.
 * @return QX_ERR_INVALID_ARGUMENT if capacity == 0.
 */
QX_API QXResult qx_snapshot_copy_all(
    QXSnapshotHistoryHandle  history,
    QXCognitiveSnapshot     *out_snapshots,
    uint32_t                 capacity,
    uint32_t                *out_written
);

/**
 * @brief Find the most recent snapshot where is_law5_compliant == QX_FALSE.
 *
 * @param history       Non-NULL history handle.
 * @param out_snapshot  Non-NULL. Receives a copy of the matching snapshot.
 * @return QX_OK if a non-compliant snapshot was found.
 * @return QX_ERR_NULL_HANDLE if history or out_snapshot is NULL.
 * @return QX_ERR_NOT_SUPPORTED if all snapshots in history are compliant.
 * @return QX_ERR_INVALID_ARGUMENT if history is empty.
 */
QX_API QXResult qx_snapshot_find_non_compliant(
    QXSnapshotHistoryHandle  history,
    QXCognitiveSnapshot     *out_snapshot
);

/**
 * @brief Clear all snapshots from the history.
 *
 * The sequence counter is NOT reset; audit continuity is preserved.
 *
 * @param history  Non-NULL history handle.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if history is NULL.
 */
QX_API QXResult qx_snapshot_clear(
    QXSnapshotHistoryHandle history
);

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Statistics
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Return aggregate statistics for the snapshot history.
 *
 * @param history    Non-NULL history handle.
 * @param out_stats  Receives a snapshot of the history's aggregate stats.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if history or out_stats is NULL.
 */
QX_API QXResult qx_snapshot_history_stats(
    QXSnapshotHistoryHandle  history,
    QXSnapshotHistoryStats  *out_stats
);

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Signal Computation Helpers
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Compute the memory clarity score for a single segment.
 *
 * Formula:
 *   clarity = clamp(1.0 - |hard_utilisation - 0.45| / 0.55, 0.0, 1.0)
 *
 * @param hard_utilisation  Ratio of used_bytes / hard_limit_bytes. Must be ≥ 0.
 * @return Clarity score in range [0.0, 1.0].
 * @note Thread-safe. Pure function.
 */
QX_API double qx_snapshot_clarity_for_utilisation(double hard_utilisation);

/**
 * @brief Compute the recency signal value from elapsed seconds.
 *
 * Formula:
 *   recency = clamp(1.0 - seconds_elapsed / QX_SNAPSHOT_RECENCY_DECAY_SEC,
 *                   0.0, 1.0)
 *
 * @param seconds_elapsed  Seconds since the previous snapshot (≥ 0.0).
 * @return Recency score in range [0.0, 1.0].
 * @note Thread-safe. Pure function.
 */
QX_API double qx_snapshot_recency_signal(double seconds_elapsed);

/**
 * @brief Compute the law-compliance signal from a health score and streak.
 *
 * Formula:
 *   base   = health_score / 100.0
 *   bonus  = min(compliance_streak, 12) / 12.0 × 0.05
 *   signal = clamp(base + bonus, 0.0, 1.0)
 *
 * @param health_score      Health score in [0.0, 100.0].
 * @param compliance_streak Number of consecutive compliant evaluations.
 * @return Compliance signal in range [0.0, 1.0].
 * @note Thread-safe. Pure function.
 */
QX_API double qx_snapshot_compliance_signal(
    double   health_score,
    uint32_t compliance_streak
);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* QXENGINE_INTELLIGENCE_QX_SNAPSHOT_H */
