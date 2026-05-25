/* ============================================================================
 * QXEngine Core – qx_segment.h
 * Owner:      Masa Bayu
 * Created:    2026-05-24
 * Description: C ABI for the QXSegment named memory region.
 *
 *              A QXSegment is a named, budgeted memory region that holds
 *              up to N QLM slots. The nine canonical segments partition
 *              the application memory grid according to the manifest.
 *
 *              Constitutional enforcement:
 *                Law 2 (Limit):       hard and soft budget checks per slot
 *                Law 3 (Pairs):       deallocation tracking and orphan
 *                                     byte detection
 *                Law 4 (Equilibrium): utilisation stats for balance checks
 *                Law 8 (Economy):     orphaned byte detection (idle > 60s)
 *
 *              Ownership:
 *                QXSegmentHandle values are owned by the QXMemloc instance.
 *                Callers must NOT destroy segment handles directly.
 *                Segments are destroyed when their owning QXMemloc is
 *                destroyed via qx_memloc_destroy().
 *
 *              Thread safety:
 *                All functions in this header are thread-safe.
 *                Internal segment lock prevents data races on concurrent
 *                allocation and deallocation.
 *
 * Repository: https://github.com/qxengine/qxengine-core
 * ============================================================================
 */

#ifndef QX_SEGMENT_H
#define QX_SEGMENT_H

#include <qxengine/core/qx_types.h>
#include <qxengine/core/qx_error.h>
#include <qxengine/core/qx_constants.h>
#include <qxengine/memory/qx_leaf.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * MARK: – Segment stats snapshot
 * Read-only snapshot of segment state at one point in time.
 * Used by QXLawEnforcer and QXTelemetry every 5 seconds.
 * ============================================================================ */

/**
 * @brief Read-only snapshot of a segment's current state.
 *
 * Populated by qx_segment_stats(). All fields reflect the segment
 * state at the moment of the call.
 *
 * segment_id is a null-terminated string matching one of the nine
 * canonical QX_SEGMENT_* identifiers defined in qx_constants.h.
 */
typedef struct QXSegmentStats {
    char        segment_id[16];       /**< Segment ID, null-terminated       */
    uint32_t    total_leaves;         /**< Total leaves (all states)         */
    uint32_t    active_leaves;        /**< Leaves in ACTIVE state            */
    uint32_t    evicted_leaves;       /**< Leaves in EVICTED state           */
    uint32_t    released_leaves;      /**< Leaves in RELEASED state          */
    uint32_t    max_slots;            /**< Maximum QLM slots for segment     */
    QXSize      used_bytes;           /**< Bytes in active leaves            */
    QXSize      soft_limit_bytes;     /**< Effective soft budget in bytes    */
    QXSize      hard_limit_bytes;     /**< Effective hard budget in bytes    */
    uint64_t    total_allocations;    /**< Cumulative allocation count       */
    uint64_t    total_deallocations;  /**< Cumulative deallocation count     */
    uint64_t    total_evictions;      /**< Cumulative eviction count         */
    double      soft_utilisation_pct; /**< used_bytes / soft_limit × 100    */
    double      hard_utilisation_pct; /**< used_bytes / hard_limit × 100    */
    double      pairs_ratio;          /**< deallocations / allocations       */
    QXSize      orphaned_bytes;       /**< Bytes idle > QX_ORPHAN_IDLE_SEC  */
    QXTierId    pressure_tier;        /**< Current pressure tier [1, 4]     */
    QXTimestamp snapshot_at_ms;       /**< Timestamp of this snapshot        */
} QXSegmentStats;

/* ============================================================================
 * MARK: – Segment API
 * ============================================================================ */

/**
 * @brief Returns a read-only stats snapshot of the segment.
 *
 * Captures the segment's current state atomically under the
 * internal segment lock. Safe to call from any thread.
 *
 * @param[in]  segment   Valid segment handle. Must not be NULL.
 * @param[out] out_stats Caller-allocated struct to populate.
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if segment or out_stats is NULL.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_segment_stats(
    QXSegmentHandle  segment,
    QXSegmentStats*  out_stats
);

/**
 * @brief Returns the segment ID string.
 *
 * [buffer.capacity] must be ≥ 16 bytes to hold the longest segment
 * ID (e.g. "QLM-TEMP" = 8 bytes + null, padded to 16 for safety).
 *
 * @param[in]     segment Valid segment handle. Must not be NULL.
 * @param[in,out] buffer  Caller-allocated string buffer (capacity ≥ 16).
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE      if segment or buffer.buffer is NULL.
 * @return QX_ERR_BUFFER_TOO_SMALL if buffer.capacity < 16.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_segment_id(
    QXSegmentHandle segment,
    char*           buffer
);

/**
 * @brief Returns the number of bytes currently in use.
 *
 * Reflects the sum of size_bytes across all ACTIVE leaves.
 *
 * @param[in]  segment    Valid segment handle. Must not be NULL.
 * @param[out] out_bytes  Written with the used byte count.
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if segment or out_bytes is NULL.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_segment_used_bytes(
    QXSegmentHandle segment,
    QXSize*         out_bytes
);

/**
 * @brief Returns the soft budget limit in bytes.
 *
 * The soft limit is the target budget derived from the manifest
 * and scaled by the device class factor.
 *
 * @param[in]  segment    Valid segment handle. Must not be NULL.
 * @param[out] out_bytes  Written with the soft limit in bytes.
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if segment or out_bytes is NULL.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_segment_soft_limit(
    QXSegmentHandle segment,
    QXSize*         out_bytes
);

/**
 * @brief Returns the hard budget limit in bytes.
 *
 * The hard limit is the absolute maximum. Allocations that would
 * exceed this limit are rejected with QX_ERR_HARD_BUDGET.
 *
 * @param[in]  segment    Valid segment handle. Must not be NULL.
 * @param[out] out_bytes  Written with the hard limit in bytes.
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if segment or out_bytes is NULL.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_segment_hard_limit(
    QXSegmentHandle segment,
    QXSize*         out_bytes
);

/**
 * @brief Returns the current pressure tier of the segment.
 *
 * Computed from soft_utilisation_pct using the tier thresholds:
 *   Tier 1: < QX_TIER_2_THRESHOLD_PCT  (70%)
 *   Tier 2: ≥ QX_TIER_2_THRESHOLD_PCT  (70%)
 *   Tier 3: ≥ QX_TIER_3_THRESHOLD_PCT  (85%)
 *   Tier 4: ≥ QX_TIER_4_THRESHOLD_PCT  (95%)
 *
 * @param[in]  segment   Valid segment handle. Must not be NULL.
 * @param[out] out_tier  Written with the current pressure tier.
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if segment or out_tier is NULL.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_segment_pressure_tier(
    QXSegmentHandle segment,
    QXTierId*       out_tier
);

/**
 * @brief Returns the current pairs ratio (deallocations / allocations).
 *
 * A ratio of 1.0 means every allocation has a matching deallocation.
 * A ratio below QX_MIN_PAIRS_RATIO (0.5) triggers a Law 3 warning.
 *
 * Returns 1.0 if no allocations have been made (no violation).
 *
 * @param[in]  segment    Valid segment handle. Must not be NULL.
 * @param[out] out_ratio  Written with the pairs ratio [0.0, 1.0+].
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if segment or out_ratio is NULL.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_segment_pairs_ratio(
    QXSegmentHandle segment,
    double*         out_ratio
);

/**
 * @brief Returns the total orphaned bytes in the segment.
 *
 * Orphaned bytes are bytes in ACTIVE leaves that have been idle
 * for more than QX_ORPHAN_IDLE_SEC (60) seconds.
 * High orphaned bytes indicate a Law 3 (Pairs) and Law 8 (Economy)
 * compliance risk.
 *
 * @param[in]  segment    Valid segment handle. Must not be NULL.
 * @param[out] out_bytes  Written with the orphaned byte count.
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if segment or out_bytes is NULL.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_segment_orphaned_bytes(
    QXSegmentHandle segment,
    QXSize*         out_bytes
);

/**
 * @brief Returns a leaf handle by slot index.
 *
 * Provides direct access to a leaf by its QLM slot index.
 * Returns NULL in out_leaf if the slot is empty.
 *
 * @param[in]  segment    Valid segment handle. Must not be NULL.
 * @param[in]  slot_index QLM slot index within the segment [0, max_slots).
 * @param[out] out_leaf   Written with the leaf handle or NULL if empty.
 *
 * @return QX_OK on success (out_leaf may be NULL for empty slot).
 * @return QX_ERR_NULL_HANDLE      if segment or out_leaf is NULL.
 * @return QX_ERR_INVALID_ARGUMENT if slot_index >= max_slots.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_segment_leaf_at_slot(
    QXSegmentHandle segment,
    uint32_t        slot_index,
    QXLeafHandle*   out_leaf
);

/**
 * @brief Returns the number of eviction candidates at the given tier.
 *
 * An eviction candidate is an ACTIVE leaf whose class allows eviction
 * at the specified pressure tier.
 *
 * @param[in]  segment    Valid segment handle. Must not be NULL.
 * @param[in]  tier       Pressure tier [2, 4]. Tier 1 has no candidates.
 * @param[out] out_count  Written with the candidate count.
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE      if segment or out_count is NULL.
 * @return QX_ERR_INVALID_ARGUMENT if tier < 2 or tier > 4.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_segment_eviction_candidate_count(
    QXSegmentHandle segment,
    QXTierId        tier,
    uint32_t*       out_count
);

/**
 * @brief Flushes leaves that are in RELEASED or EVICTED state.
 *
 * Removes stale leaf records from the segment registry, freeing
 * slot capacity without affecting ACTIVE leaves.
 * Called periodically by QXMemloc during pressure evaluation.
 *
 * @param[in]  segment      Valid segment handle. Must not be NULL.
 * @param[out] out_flushed  Written with the number of leaves flushed.
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if segment or out_flushed is NULL.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_segment_flush_terminated(
    QXSegmentHandle segment,
    uint32_t*       out_flushed
);

/**
 * @brief Returns QX_TRUE if the segment has available slot capacity.
 *
 * Equivalent to: active_leaves < max_slots
 *
 * @param[in] segment Valid segment handle. Must not be NULL.
 * @return QX_TRUE  if slots are available.
 * @return QX_FALSE if the segment is at capacity or handle is NULL.
 *
 * @note Thread-safe.
 */
QX_API QXBool qx_segment_has_capacity(QXSegmentHandle segment);

/**
 * @brief Returns QX_TRUE if used_bytes exceeds the soft limit.
 *
 * @param[in] segment Valid segment handle. Must not be NULL.
 * @return QX_TRUE  if used_bytes > soft_limit_bytes.
 * @return QX_FALSE if within soft limit or handle is NULL.
 *
 * @note Thread-safe.
 */
QX_API QXBool qx_segment_over_soft_limit(QXSegmentHandle segment);

/**
 * @brief Returns QX_TRUE if used_bytes exceeds the hard limit.
 *
 * @param[in] segment Valid segment handle. Must not be NULL.
 * @return QX_TRUE  if used_bytes > hard_limit_bytes.
 * @return QX_FALSE if within hard limit or handle is NULL.
 *
 * @note Thread-safe.
 */
QX_API QXBool qx_segment_over_hard_limit(QXSegmentHandle segment);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* QX_SEGMENT_H */
