// =============================================================================
// QXEngine Core – include/qxengine/intelligence/qx_snapshot_types.h
// Owner : Masa Bayu
// Created: 2026-05-24
// Repo   : https://github.com/qxengine/qxengine-core
// Description: Types layer – constants, signal enum, signal weights, and all
//              snapshot data structures consumed by the intelligence subsystem
//              and its callers (telemetry, law enforcer, platform adapters).
// =============================================================================

#ifndef QXENGINE_INTELLIGENCE_QX_SNAPSHOT_TYPES_H
#define QXENGINE_INTELLIGENCE_QX_SNAPSHOT_TYPES_H

#include "qxengine/core/qx_types.h"
#include "qxengine/core/qx_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Constants
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Maximum number of snapshots retained in the rolling history.
 *
 * At QX_SNAPSHOT_INTERVAL_SEC (5 s) this represents 60 minutes of history.
 * When the history is full the oldest snapshot is overwritten (ring buffer).
 */
#define QX_SNAPSHOT_HISTORY_MAX         720u

/**
 * @brief Number of cognitive signals contributing to the knowledge score.
 */
#define QX_SNAPSHOT_SIGNAL_COUNT        5u

/**
 * @brief Minimum knowledge score for Law 5 compliance.
 */
#define QX_SNAPSHOT_MIN_SCORE           90.0

/**
 * @brief Snapshot recency decay rate (seconds).
 *
 * The recency signal decays linearly from 1.0 at t=0 to 0.0 at t=decay.
 * Default: 2 × QX_SNAPSHOT_INTERVAL_SEC = 10 s.
 */
#define QX_SNAPSHOT_RECENCY_DECAY_SEC   10.0

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXCognitiveSignal
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Enumeration of the five cognitive signals.
 *
 * Used to index into QXCognitiveSnapshot.signal_values[].
 */
typedef enum QXCognitiveSignal {
    /** Memory utilisation clarity across all nine segments. Weight: 0.25. */
    QX_SIGNAL_MEMORY_CLARITY        = 0,

    /** Pressure tier stability (inverse of recent tier variance). Weight: 0.20. */
    QX_SIGNAL_PRESSURE_STABILITY    = 1,

    /** Law compliance streak score from QXLawEnforcer history. Weight: 0.25. */
    QX_SIGNAL_LAW_COMPLIANCE        = 2,

    /** Native capability coverage ratio from QXNativeRegistry. Weight: 0.15. */
    QX_SIGNAL_NATIVE_COVERAGE       = 3,

    /** Snapshot recency (decays linearly with time since last capture). Weight: 0.15. */
    QX_SIGNAL_RECENCY               = 4
} QXCognitiveSignal;

/**
 * @brief Weights for each cognitive signal (index = QXCognitiveSignal value).
 *
 * Must sum to exactly 1.0. Authoritative for spec v1.0.0.
 */
static const double QX_SIGNAL_WEIGHTS[QX_SNAPSHOT_SIGNAL_COUNT] = {
    0.25,   /* QX_SIGNAL_MEMORY_CLARITY     */
    0.20,   /* QX_SIGNAL_PRESSURE_STABILITY */
    0.25,   /* QX_SIGNAL_LAW_COMPLIANCE     */
    0.15,   /* QX_SIGNAL_NATIVE_COVERAGE    */
    0.15    /* QX_SIGNAL_RECENCY            */
};

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXSegmentClarity
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Per-segment memory clarity entry within a snapshot.
 *
 * Memory clarity is 1.0 when utilisation is in the healthy band [20 %, 70 %]
 * and degrades toward 0.0 as utilisation approaches 0 % (starvation) or
 * 100 % (saturation).
 */
typedef struct QXSegmentClarity {
    /** Null-terminated segment ID (max 16 bytes including null). */
    char    segment_id[16];

    /**
     * @brief Segment clarity score in range [0.0, 1.0].
     *
     * Formula:
     *   u       = used_bytes / hard_limit_bytes   (utilisation, [0, ∞))
     *   clarity = clamp(1.0 - |u - 0.45| / 0.55, 0.0, 1.0)
     *
     * Optimal clarity (1.0) at u ≈ 0.45 (45 % of hard limit).
     */
    double  clarity_score;

    /** Hard-limit utilisation at snapshot time (used_bytes / hard_limit_bytes). */
    double  hard_utilisation;

    /** Pressure tier at snapshot time (1–4). */
    QXTierId pressure_tier;
} QXSegmentClarity;

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXCognitiveSnapshot
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief A single cognitive intelligence snapshot.
 *
 * Captures a complete view of engine intelligence at a point in time.
 * Produced by qx_snapshot_capture() and stored in QXSnapshotHistory.
 */
typedef struct QXCognitiveSnapshot {
    // ── Identity ─────────────────────────────────────────────────────────────

    /** Monotonically increasing sequence number. Starts at 1. */
    QXSequence      sequence;

    /** Wall-clock timestamp when this snapshot was captured (ms since epoch). */
    QXTimestamp     captured_at_ms;

    // ── Knowledge Score ───────────────────────────────────────────────────────

    /**
     * @brief Composite cognitive knowledge score (0.0 – 100.0).
     *
     * knowledge_score = Σ (signal_values[i] × QX_SIGNAL_WEIGHTS[i]) × 100.0
     *
     * Law 5 compliance thresholds:
     *   ≥ 90.0 → COMPLIANT   ≥ 70.0 → WARNING
     *   ≥ 50.0 → CRITICAL    <  50.0 → FATAL
     */
    double          knowledge_score;

    /**
     * @brief Whether this snapshot meets the Law 5 minimum threshold.
     *
     * QX_TRUE when knowledge_score ≥ QX_SNAPSHOT_MIN_SCORE (90.0).
     */
    QXBool          is_law5_compliant;

    // ── Raw Signal Values ─────────────────────────────────────────────────────

    /**
     * @brief Raw signal values in range [0.0, 1.0].
     *
     * Index corresponds to QXCognitiveSignal enum:
     *   [0] QX_SIGNAL_MEMORY_CLARITY     [1] QX_SIGNAL_PRESSURE_STABILITY
     *   [2] QX_SIGNAL_LAW_COMPLIANCE     [3] QX_SIGNAL_NATIVE_COVERAGE
     *   [4] QX_SIGNAL_RECENCY
     */
    double          signal_values[QX_SNAPSHOT_SIGNAL_COUNT];

    /**
     * @brief Weighted contributions of each signal to the final score.
     *
     * weighted_contributions[i] = signal_values[i] × QX_SIGNAL_WEIGHTS[i]
     * These sum to knowledge_score / 100.0.
     */
    double          weighted_contributions[QX_SNAPSHOT_SIGNAL_COUNT];

    // ── Memory Intelligence ───────────────────────────────────────────────────

    /** Per-segment clarity entries. Valid entries: segment_clarity_count. */
    QXSegmentClarity    segment_clarity[QX_SEGMENT_COUNT];

    /** Number of valid entries in @p segment_clarity. */
    uint32_t            segment_clarity_count;

    /**
     * @brief Composite memory clarity (average across all segments).
     *
     * Equals signal_values[QX_SIGNAL_MEMORY_CLARITY].
     */
    double              composite_memory_clarity;

    /** Total engine-wide memory usage at snapshot time (bytes). */
    QXSize              total_used_bytes;

    // ── Pressure Intelligence ─────────────────────────────────────────────────

    /** Composite pressure tier at snapshot time (1–4). */
    QXTierId            composite_pressure_tier;

    /**
     * @brief Pressure tier standard deviation over the last N snapshots.
     *
     * Low variance → stable pressure → high QX_SIGNAL_PRESSURE_STABILITY.
     */
    double              pressure_tier_variance;

    // ── Law Compliance Intelligence ───────────────────────────────────────────

    /**
     * @brief Health score from the most recent QXLawEnforcer evaluation.
     *
     * Feeds into QX_SIGNAL_LAW_COMPLIANCE as health_score / 100.0.
     */
    double              latest_law_health_score;

    /**
     * @brief Consecutive fully-compliant evaluations before capture.
     *
     * A streak ≥ 12 contributes a bonus of up to +0.05 to
     * QX_SIGNAL_LAW_COMPLIANCE, capped at 1.0.
     */
    uint32_t            compliance_streak;

    // ── Native Coverage Intelligence ──────────────────────────────────────────

    /**
     * @brief Active native capabilities as a fraction of declared capabilities.
     *
     * native_coverage = active_capability_count / declared_capability_count.
     * 1.0 when declared_capability_count == 0 (no requirements declared).
     */
    double              native_coverage_ratio;

    /** Number of native capabilities declared in the manifest. */
    uint32_t            declared_capability_count;

    /** Number of declared capabilities confirmed active at snapshot time. */
    uint32_t            active_capability_count;

    // ── Recency ───────────────────────────────────────────────────────────────

    /**
     * @brief Seconds elapsed since the immediately preceding snapshot.
     *
     * For the very first snapshot this field is 0.0. Feeds into
     * QX_SIGNAL_RECENCY via:
     *   recency = max(0.0, 1.0 - elapsed / QX_SNAPSHOT_RECENCY_DECAY_SEC)
     */
    double              seconds_since_previous;
} QXCognitiveSnapshot;

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXSnapshotInput
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Input data required to compute a QXCognitiveSnapshot.
 *
 * The platform adapter populates this from OS-facing subsystems and passes
 * it to qx_snapshot_capture(). All derived fields are computed internally.
 */
typedef struct QXSnapshotInput {
    /** Per-segment hard-limit utilisation ratios (used / hard_limit). */
    double      segment_utilisations[QX_SEGMENT_COUNT];

    /** Segment IDs matching @p segment_utilisations (null-terminated, max 16 B). */
    char        segment_ids[QX_SEGMENT_COUNT][16];

    /** Number of valid entries in @p segment_utilisations. */
    uint32_t    segment_count;

    /** Total engine-wide resident bytes at capture time. */
    QXSize      total_used_bytes;

    /** Composite pressure tier at capture time (1–4). */
    QXTierId    composite_pressure_tier;

    /**
     * @brief Health score from the most recent QXLawEnforcer evaluation.
     *
     * Must be in range [0.0, 100.0].
     */
    double      latest_law_health_score;

    /** Number of consecutive fully-compliant evaluations. */
    uint32_t    compliance_streak;

    /**
     * @brief Fraction of active native capabilities (0.0 – 1.0).
     *
     * Computed by the platform adapter from QXNativeRegistry.
     */
    double      native_coverage_ratio;

    /** Total declared native capabilities at capture time. */
    uint32_t    declared_capability_count;

    /** Total active native capabilities at capture time. */
    uint32_t    active_capability_count;

    /** Wall-clock timestamp when this input was assembled (ms since epoch). */
    QXTimestamp captured_at_ms;
} QXSnapshotInput;

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXSnapshotHistoryStats
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Aggregate statistics for a QXSnapshotHistory instance.
 */
typedef struct QXSnapshotHistoryStats {
    /** Number of snapshots currently stored in history. */
    uint32_t        snapshot_count;

    /** Total snapshots ever captured (monotonically increasing). */
    uint64_t        total_captured;

    /** Highest knowledge_score ever recorded. */
    double          peak_knowledge_score;

    /** Lowest knowledge_score ever recorded. */
    double          trough_knowledge_score;

    /** Running mean of knowledge_score across all stored snapshots. */
    double          mean_knowledge_score;

    /** Most recent knowledge_score. */
    double          last_knowledge_score;

    /** Number of stored snapshots where is_law5_compliant == QX_FALSE. */
    uint32_t        non_compliant_count;

    /** Timestamp of the oldest stored snapshot (ms). */
    QXTimestamp     oldest_snapshot_ms;

    /** Timestamp of the most recent stored snapshot (ms). */
    QXTimestamp     latest_snapshot_ms;

    /**
     * @brief Seconds elapsed since the most recent snapshot was captured.
     *
     * If this value exceeds QX_SNAPSHOT_INTERVAL_SEC the intelligence layer
     * may be stalled – Law 5 will raise a WARNING.
     */
    double          seconds_since_last_capture;
} QXSnapshotHistoryStats;

/* Opaque handle for QXSnapshotHistory */
typedef void* QXSnapshotHistoryHandle;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* QXENGINE_INTELLIGENCE_QX_SNAPSHOT_TYPES_H */
