/* ============================================================================
 * QXEngine Core – qx_memloc.h
 * Owner:      Masa Bayu
 * Created:    2026-05-24
 * Description: C ABI for the QXMemloc central allocation engine.
 *
 *              QXMemloc is the single allocation authority for the QXEngine
 *              memory grid. It manages QX_SLOT_COUNT (60) QLM slots
 *              distributed across QX_SEGMENT_COUNT (9) constitutional
 *              segments.
 *
 *              Slot distribution:
 *                QLM-CORE, QLM-UI, QLM-DATA, QLM-IMG, QLM-NET, QLM-AI
 *                  → QX_SLOTS_HIGH_PRIORITY (7) slots each
 *                QLM-SEC, QLM-LOG, QLM-TEMP
 *                  → QX_SLOTS_LOW_PRIORITY (6) slots each
 *                Total: (6 × 7) + (3 × 6) = 60 slots
 *
 *              Constitutional enforcement:
 *                Law 1 (Pattern):     label validation on every allocation
 *                Law 2 (Limit):       segment existence, slot capacity,
 *                                     soft/hard budget enforcement,
 *                                     eviction cascade
 *                Law 3 (Pairs):       deallocation tracking, double-free
 *                                     detection
 *                Law 4 (Equilibrium): post-allocation imbalance detection
 *
 *              Ownership:
 *                QXMemlocHandle is created by qx_memloc_create() and
 *                destroyed by qx_memloc_destroy(). The caller owns the
 *                handle. QXLeafHandle values returned by
 *                qx_memloc_allocate() are owned by the QXMemloc instance
 *                and must not be individually destroyed by the caller.
 *
 *              Thread safety:
 *                All public functions are thread-safe. Segment-level
 *                locks prevent data races on concurrent allocation and
 *                deallocation across different segments. Allocations
 *                within the same segment are serialised.
 *
 * Repository: https://github.com/qxengine/qxengine-core
 * ============================================================================
 */

#ifndef QX_MEMLOC_H
#define QX_MEMLOC_H

#include <qxengine/core/qx_types.h>
#include <qxengine/core/qx_error.h>
#include <qxengine/core/qx_constants.h>
#include <qxengine/memory/qx_leaf.h>
#include <qxengine/memory/qx_segment.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * MARK: – Memloc configuration
 * ============================================================================ */

/**
 * @brief Per-segment budget configuration for QXMemloc initialisation.
 *
 * Passed as an array of QX_SEGMENT_COUNT entries to qx_memloc_create().
 * The segment_id must match one of the nine canonical QX_SEGMENT_*
 * identifiers defined in qx_constants.h.
 *
 * Effective bytes are the manifest limits scaled by the device class
 * factor (QX_SCALE_HIGH_END, QX_SCALE_MID_RANGE, QX_SCALE_ENTRY_LEVEL).
 */
typedef struct QXSegmentConfig {
    char     segment_id[16];          /**< Segment ID, null-terminated      */
    QXSize   effective_soft_bytes;    /**< Scaled soft limit in bytes        */
    QXSize   effective_hard_bytes;    /**< Scaled hard limit in bytes        */
    uint32_t max_slots;               /**< Maximum QLM slots for segment     */
} QXSegmentConfig;

/* ============================================================================
 * MARK: – Allocation result
 * ============================================================================ */

/**
 * @brief Result of a successful qx_memloc_allocate() call.
 *
 * Contains the new leaf handle and post-allocation state information
 * for the segment. Used by QIADirector to assess pressure after each
 * allocation without a separate stats query.
 */
typedef struct QXAllocResult {
    QXLeafHandle leaf;                /**< New leaf handle (owned by memloc) */
    char         segment_id[16];      /**< Segment that received the leaf    */
    uint32_t     slot_index;          /**< QLM slot index assigned           */
    QXSize       bytes_allocated;     /**< Actual bytes allocated            */
    double       soft_pressure_pct;   /**< Segment soft utilisation after    */
    QXTierId     pressure_tier;       /**< Segment pressure tier after       */
    uint32_t     evicted_count;       /**< Leaves evicted during cascade     */
} QXAllocResult;

/* ============================================================================
 * MARK: – Equilibrium result
 * ============================================================================ */

/**
 * @brief Result of a qx_memloc_check_equilibrium() call.
 *
 * Reports the balance state of the nine segments for Law 4 evaluation.
 * Populated with per-segment utilisation percentages and overall balance
 * assessment.
 */
typedef struct QXEquilibriumResult {
    double   utilisation_pcts[9];     /**< Soft utilisation per segment      */
    char     segment_ids[9][16];      /**< Segment IDs matching above array  */
    uint32_t overloaded_count;        /**< Segments above overload threshold */
    uint32_t starved_count;           /**< Segments below starvation thresh  */
    QXBool   is_balanced;             /**< True when no over/starvation      */
} QXEquilibriumResult;

/* ============================================================================
 * MARK: – Memloc lifecycle API
 * ============================================================================ */

/**
 * @brief Creates a new QXMemloc instance with the given segment budgets.
 *
 * Initialises all nine constitutional segments with the provided
 * budget configurations. Each segment_id in [configs] must be unique
 * and match one of the nine canonical segment IDs.
 *
 * @param[in]  configs      Array of QX_SEGMENT_COUNT segment configurations.
 *                          Must not be NULL.
 * @param[in]  config_count Must equal QX_SEGMENT_COUNT (9).
 * @param[out] out_memloc   Written with the new QXMemlocHandle on success.
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE      if configs or out_memloc is NULL.
 * @return QX_ERR_INVALID_ARGUMENT if config_count != QX_SEGMENT_COUNT.
 * @return QX_ERR_UNKNOWN_SEGMENT  if any segment_id is not canonical.
 * @return QX_ERR_INTERNAL         on memory allocation failure.
 *
 * @note Not thread-safe. Call from one thread at startup.
 */
QX_API QXResult qx_memloc_create(
    const QXSegmentConfig* configs,
    uint32_t               config_count,
    QXMemlocHandle*        out_memloc
);

/**
 * @brief Destroys a QXMemloc instance and releases all owned resources.
 *
 * Releases all segments, leaves, and internal state. All QXLeafHandle
 * and QXSegmentHandle values obtained from this memloc become invalid
 * after this call.
 *
 * Passing NULL is a no-op.
 *
 * @param[in] memloc QXMemloc handle to destroy. May be NULL.
 *
 * @note Not thread-safe. Call from one thread at shutdown
 *               after all other threads have stopped using the memloc.
 */
QX_API void qx_memloc_destroy(QXMemlocHandle memloc);

/* ============================================================================
 * MARK: – Allocation API
 * ============================================================================ */

/**
 * @brief Allocates a new leaf in the specified segment.
 *
 * Enforces all memory constitutional laws on every call:
 *
 * Law 1 (Pattern):
 *   - [label] must not be NULL or blank
 *   - [label] length must be in [QX_LABEL_MIN_LENGTH, QX_LABEL_MAX_LENGTH]
 *
 * Law 2 (Limit):
 *   - [segment_id] must be one of the nine canonical segment IDs
 *   - Segment must have available slot capacity
 *   - Allocation must not exceed the hard budget
 *   - If soft budget would be exceeded, an eviction cascade is attempted
 *     before the allocation proceeds
 *
 * Law 3 (Pairs):
 *   - Allocation is recorded for pairs ratio tracking
 *
 * Law 4 (Equilibrium):
 *   - Post-allocation equilibrium check is performed
 *   - Imbalance is logged but does not block the allocation
 *
 * @param[in]  memloc      Valid QXMemloc handle. Must not be NULL.
 * @param[in]  label       Non-null UTF-8 label string, length [3, 128].
 * @param[in]  segment_id  One of the nine canonical QLM-* segment IDs.
 * @param[in]  leaf_class  Leaf class (QX_LEAF_CLASS_A through D).
 * @param[in]  size_bytes  Allocation size in bytes. Must be > 0.
 * @param[out] out_result  Caller-allocated QXAllocResult to populate.
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE      if memloc, label, segment_id, or
 *                                  out_result is NULL.
 * @return QX_ERR_LABEL_BLANK      if label is blank or whitespace.
 * @return QX_ERR_LABEL_TOO_SHORT  if label length < QX_LABEL_MIN_LENGTH.
 * @return QX_ERR_LABEL_TOO_LONG   if label length > QX_LABEL_MAX_LENGTH.
 * @return QX_ERR_UNKNOWN_SEGMENT  if segment_id is not canonical.
 * @return QX_ERR_NO_SLOT          if segment is at slot capacity.
 * @return QX_ERR_HARD_BUDGET      if allocation exceeds hard limit.
 * @return QX_ERR_INVALID_ARGUMENT if size_bytes == 0.
 * @return QX_ERR_INVALID_LEAF_CLASS if leaf_class > QX_LEAF_CLASS_D.
 *
 * @note Thread-safe. Concurrent allocations in different
 *               segments proceed in parallel. Same-segment allocations
 *               are serialised via the segment lock.
 */
QX_API QXResult qx_memloc_allocate(
    QXMemlocHandle   memloc,
    const char*      label,
    const char*      segment_id,
    QXLeafClassId    leaf_class,
    QXSize           size_bytes,
    QXAllocResult*   out_result
);

/**
 * @brief Deallocates a leaf by its UUID string ID.
 *
 * Searches all nine segments for the leaf with the given ID and
 * releases it. The QXLeafHandle becomes invalid after this call.
 *
 * Law 3 (Pairs):
 *   - Double-free returns QX_ERR_DOUBLE_FREE immediately.
 *   - Deallocation is recorded for pairs ratio tracking.
 *
 * @param[in]  memloc    Valid QXMemloc handle. Must not be NULL.
 * @param[in]  leaf_id   Null-terminated UUID string. Must not be NULL.
 * @param[out] out_bytes Written with the bytes reclaimed on success.
 *                       May be NULL if the caller does not need this.
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE   if memloc or leaf_id is NULL.
 * @return QX_ERR_LEAF_NOT_FOUND if no leaf with leaf_id exists.
 * @return QX_ERR_DOUBLE_FREE   if the leaf was already deallocated.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_memloc_deallocate(
    QXMemlocHandle memloc,
    const char*    leaf_id,
    QXSize*        out_bytes
);

/**
 * @brief Updates the LRU timestamp of a leaf by UUID string ID.
 *
 * Searches all nine segments for the leaf with the given ID and
 * calls touch() on it. No-op if the leaf is not in ACTIVE state.
 *
 * @param[in] memloc  Valid QXMemloc handle. Must not be NULL.
 * @param[in] leaf_id Null-terminated UUID string. Must not be NULL.
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE    if memloc or leaf_id is NULL.
 * @return QX_ERR_LEAF_NOT_FOUND if no leaf with leaf_id exists.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_memloc_touch(
    QXMemlocHandle memloc,
    const char*    leaf_id
);

/* ============================================================================
 * MARK: – Stats and query API
 * ============================================================================ */

/**
 * @brief Returns a stats snapshot for a specific segment.
 *
 * @param[in]  memloc      Valid QXMemloc handle. Must not be NULL.
 * @param[in]  segment_id  Canonical segment ID. Must not be NULL.
 * @param[out] out_stats   Caller-allocated QXSegmentStats to populate.
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE     if memloc, segment_id, or out_stats is NULL.
 * @return QX_ERR_UNKNOWN_SEGMENT if segment_id is not canonical.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_memloc_segment_stats(
    QXMemlocHandle  memloc,
    const char*     segment_id,
    QXSegmentStats* out_stats
);

/**
 * @brief Returns stats snapshots for all nine segments.
 *
 * Populates [out_stats] array with QX_SEGMENT_COUNT entries in the
 * canonical segment order:
 *   [0] QLM-CORE  [1] QLM-UI   [2] QLM-DATA  [3] QLM-IMG
 *   [4] QLM-NET   [5] QLM-AI   [6] QLM-SEC   [7] QLM-LOG
 *   [8] QLM-TEMP
 *
 * @param[in]  memloc      Valid QXMemloc handle. Must not be NULL.
 * @param[out] out_stats   Caller-allocated array of QX_SEGMENT_COUNT
 *                         QXSegmentStats structs. Must not be NULL.
 * @param[in]  count       Must equal QX_SEGMENT_COUNT (9).
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE      if memloc or out_stats is NULL.
 * @return QX_ERR_INVALID_ARGUMENT if count != QX_SEGMENT_COUNT.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_memloc_all_stats(
    QXMemlocHandle  memloc,
    QXSegmentStats* out_stats,
    uint32_t        count
);

/**
 * @brief Returns the total bytes in use across all nine segments.
 *
 * @param[in]  memloc     Valid QXMemloc handle. Must not be NULL.
 * @param[out] out_bytes  Written with the total used byte count.
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if memloc or out_bytes is NULL.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_memloc_total_used_bytes(
    QXMemlocHandle memloc,
    QXSize*        out_bytes
);

/**
 * @brief Returns a segment handle by segment ID.
 *
 * The returned handle is owned by the QXMemloc instance.
 * Do not destroy it directly.
 *
 * @param[in]  memloc      Valid QXMemloc handle. Must not be NULL.
 * @param[in]  segment_id  Canonical segment ID. Must not be NULL.
 * @param[out] out_segment Written with the segment handle on success.
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE     if memloc, segment_id, or out_segment NULL.
 * @return QX_ERR_UNKNOWN_SEGMENT if segment_id is not canonical.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_memloc_get_segment(
    QXMemlocHandle   memloc,
    const char*      segment_id,
    QXSegmentHandle* out_segment
);

/* ============================================================================
 * MARK: – Equilibrium API (Law 4)
 * ============================================================================ */

/**
 * @brief Evaluates the equilibrium balance of all nine segments.
 *
 * Computes the soft utilisation percentage for each segment and
 * identifies overloaded (> QX_EQUILIBRIUM_OVERLOAD_PCT) and starved
 * (< QX_EQUILIBRIUM_STARVE_PCT) segments.
 *
 * Called by QXLawEnforcer during every constitutional evaluation cycle.
 *
 * @param[in]  memloc     Valid QXMemloc handle. Must not be NULL.
 * @param[out] out_result Caller-allocated QXEquilibriumResult to populate.
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if memloc or out_result is NULL.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_memloc_check_equilibrium(
    QXMemlocHandle       memloc,
    QXEquilibriumResult* out_result
);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* QX_MEMLOC_H */
