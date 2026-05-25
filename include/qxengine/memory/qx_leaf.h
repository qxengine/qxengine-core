/* ============================================================================
 * QXEngine Core – qx_leaf.h
 * Owner:      Masa Bayu
 * Created:    2026-05-24
 * Description: C ABI for the QXLeaf atomic allocation unit.
 *
 *              A QXLeaf is the smallest named memory allocation in the
 *              QXEngine memory grid. Every allocation must carry a label
 *              (Law 1 – Pattern) and belongs to exactly one segment and
 *              one QLM slot.
 *
 *              Constitutional enforcement:
 *                Law 1 (Pattern):     label required, length [3, 128]
 *                Law 2 (Limit):       Class A leaves are never evicted
 *                Law 3 (Pairs):       double-free detection via state machine
 *                Law 5 (Knowledge):   immutable transition history
 *
 *              Lifecycle:
 *                ACTIVE → EVICTED → RELEASED
 *                ACTIVE →           RELEASED  (explicit deallocation)
 *
 *              Ownership:
 *                QXLeafHandle values are owned by the QXMemloc instance
 *                that created them. Callers must NOT call qx_leaf_destroy()
 *                directly. Use qx_memloc_deallocate() instead.
 *
 *              Thread safety:
 *                All functions in this header are thread-safe when called
 *                through the QXMemloc or QXEngine API.
 *                Direct leaf handle access requires external locking.
 *
 * Repository: https://github.com/qxengine/qxengine-core
 * ============================================================================
 */

#ifndef QX_LEAF_H
#define QX_LEAF_H

#include <qxengine/core/qx_types.h>
#include <qxengine/core/qx_error.h>
#include <qxengine/core/qx_constants.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * MARK: – Leaf lifecycle state
 * ============================================================================ */

/**
 * @brief Leaf lifecycle state.
 *
 * Transitions are strictly ordered and irreversible:
 *   ACTIVE → EVICTED  (pressure eviction)
 *   ACTIVE → RELEASED (explicit deallocation)
 *   EVICTED → RELEASED (post-eviction cleanup)
 *
 * A leaf in RELEASED state is invalid for all operations.
 * Attempting to release a RELEASED leaf returns QX_ERR_DOUBLE_FREE.
 */
typedef enum QXLeafState {
    QX_LEAF_STATE_ACTIVE   = 0,  /**< Allocated and in use              */
    QX_LEAF_STATE_EVICTED  = 1,  /**< Evicted by pressure coordinator   */
    QX_LEAF_STATE_RELEASED = 2   /**< Explicitly deallocated            */
} QXLeafState;

/* ============================================================================
 * MARK: – Leaf transition record
 * Stored immutably in the leaf for Law 5 (Knowledge) compliance.
 * ============================================================================ */

/**
 * @brief A single lifecycle state transition record.
 *
 * The engine records every state transition for audit purposes.
 * Transitions are immutable once recorded.
 */
typedef struct QXLeafTransition {
    QXLeafState  from;           /**< Previous state (255 = initial)    */
    QXLeafState  to;             /**< New state                         */
    QXTimestamp  timestamp_ms;   /**< Unix epoch milliseconds           */
    char         reason[64];     /**< Null-terminated reason string     */
} QXLeafTransition;

/* ============================================================================
 * MARK: – Leaf info struct
 * Read-only snapshot of leaf state for inspection and telemetry.
 * ============================================================================ */

/**
 * @brief Read-only snapshot of a leaf's current state.
 *
 * Populated by qx_leaf_info(). All fields are valid at the moment
 * of the call but may change immediately after in a concurrent context.
 *
 * The leaf_id field is a null-terminated UUID string (36 bytes + null).
 * The label field is a null-terminated UTF-8 string (≤ 128 bytes + null).
 * The segment_id field is one of the nine canonical QLM-* identifiers.
 */
typedef struct QXLeafInfo {
    char            leaf_id[37];          /**< UUID string, null-terminated    */
    char            label[129];           /**< Label string, null-terminated   */
    char            segment_id[16];       /**< Segment ID, null-terminated     */
    QXLeafClassId   leaf_class;           /**< Class A/B/C/D                   */
    QXLeafState     state;                /**< Current lifecycle state         */
    QXSize          size_bytes;           /**< Allocation size in bytes        */
    uint32_t        slot_index;           /**< QLM slot index [0, 59]          */
    QXTimestamp     allocated_at_ms;      /**< Allocation timestamp            */
    QXTimestamp     last_accessed_at_ms;  /**< Last LRU touch timestamp        */
    uint64_t        touch_count;          /**< Number of LRU touch calls       */
    uint64_t        age_seconds;          /**< Seconds since allocation        */
    uint64_t        idle_seconds;         /**< Seconds since last access       */
    uint32_t        transition_count;     /**< Number of state transitions     */
    QXBool          is_evictable;         /**< True if class allows eviction   */
} QXLeafInfo;

/* ============================================================================
 * MARK: – Leaf API
 * ============================================================================ */

/**
 * @brief Reads the current state information of a leaf.
 *
 * Populates [out_info] with a snapshot of the leaf's current state.
 * This is a read-only operation and does not modify the leaf.
 *
 * @param[in]  leaf     Valid leaf handle. Must not be NULL.
 * @param[out] out_info Caller-allocated struct to populate.
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if leaf or out_info is NULL.
 *
 * @note Thread-safe for read. Requires external lock for
 *       consistency guarantees in concurrent contexts.
 */
QX_API QXResult qx_leaf_info(
    QXLeafHandle  leaf,
    QXLeafInfo*   out_info
);

/**
 * @brief Returns the current lifecycle state of a leaf.
 *
 * @param[in]  leaf      Valid leaf handle. Must not be NULL.
 * @param[out] out_state Written with the current QXLeafState.
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if leaf or out_state is NULL.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_leaf_state(
    QXLeafHandle  leaf,
    QXLeafState*  out_state
);

/**
 * @brief Returns the leaf ID as a null-terminated UUID string.
 *
 * Writes up to [buffer.capacity - 1] bytes into [buffer.buffer]
 * and null-terminates the result. Sets [buffer.length] to the
 * number of bytes written excluding the null terminator.
 *
 * A UUID string is always 36 bytes. [buffer.capacity] must be ≥ 37.
 *
 * @param[in]     leaf    Valid leaf handle. Must not be NULL.
 * @param[in,out] buffer  Caller-allocated string buffer (capacity ≥ 37).
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE      if leaf or buffer.buffer is NULL.
 * @return QX_ERR_BUFFER_TOO_SMALL if buffer.capacity < 37.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_leaf_id(
    QXLeafHandle   leaf,
    char*          buffer
);

/**
 * @brief Returns the leaf label as a null-terminated UTF-8 string.
 *
 * [buffer.capacity] must be ≥ QX_LABEL_MAX_LENGTH + 1 (129 bytes).
 *
 * @param[in]     leaf    Valid leaf handle. Must not be NULL.
 * @param[in,out] buffer  Caller-allocated string buffer (capacity ≥ 129).
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE      if leaf or buffer.buffer is NULL.
 * @return QX_ERR_BUFFER_TOO_SMALL if buffer.capacity < 129.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_leaf_label(
    QXLeafHandle   leaf,
    char*          buffer
);

/**
 * @brief Returns the allocation size of the leaf in bytes.
 *
 * @param[in]  leaf       Valid leaf handle. Must not be NULL.
 * @param[out] out_bytes  Written with the size in bytes.
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if leaf or out_bytes is NULL.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_leaf_size_bytes(
    QXLeafHandle  leaf,
    QXSize*       out_bytes
);

/**
 * @brief Returns the leaf class.
 *
 * @param[in]  leaf       Valid leaf handle. Must not be NULL.
 * @param[out] out_class  Written with the QXLeafClassId.
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if leaf or out_class is NULL.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_leaf_class(
    QXLeafHandle   leaf,
    QXLeafClassId* out_class
);

/**
 * @brief Updates the LRU timestamp of the leaf.
 *
 * Marks the leaf as recently accessed, preventing it from being
 * selected as an eviction candidate before less-recently-used leaves.
 *
 * Only valid for leaves in QX_LEAF_STATE_ACTIVE state.
 * Calling touch on an EVICTED or RELEASED leaf is a no-op.
 *
 * @param[in] leaf Valid leaf handle. Must not be NULL.
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if leaf is NULL.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_leaf_touch(QXLeafHandle leaf);

/**
 * @brief Reads a state transition record by index.
 *
 * Transition records are immutable and indexed from 0 (oldest)
 * to transition_count - 1 (most recent).
 *
 * @param[in]  leaf           Valid leaf handle. Must not be NULL.
 * @param[in]  index          Zero-based transition index.
 * @param[out] out_transition  Caller-allocated struct to populate.
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE      if leaf or out_transition is NULL.
 * @return QX_ERR_INVALID_ARGUMENT if index ≥ transition_count.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_leaf_transition(
    QXLeafHandle      leaf,
    uint32_t          index,
    QXLeafTransition* out_transition
);

/**
 * @brief Returns the number of state transitions recorded for this leaf.
 *
 * @param[in]  leaf        Valid leaf handle. Must not be NULL.
 * @param[out] out_count   Written with the transition count.
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if leaf or out_count is NULL.
 *
 * @note Thread-safe.
 */
QX_API QXResult qx_leaf_transition_count(
    QXLeafHandle  leaf,
    uint32_t*     out_count
);

/**
 * @brief Returns QX_TRUE if the leaf is currently active.
 *
 * Convenience wrapper around qx_leaf_state().
 *
 * @param[in] leaf Valid leaf handle. Must not be NULL.
 * @return QX_TRUE if state == QX_LEAF_STATE_ACTIVE.
 * @return QX_FALSE if leaf is NULL or state != ACTIVE.
 *
 * @note Thread-safe.
 */
QX_API QXBool qx_leaf_is_active(QXLeafHandle leaf);

/**
 * @brief Returns QX_TRUE if the leaf class allows eviction at the
 *        given pressure tier.
 *
 * | Class | Tier 2 | Tier 3 | Tier 4 |
 * |-------|--------|--------|--------|
 * |   A   |   No   |   No   |   No   |
 * |   B   |   No   |   No   |  Yes   |
 * |   C   |   No   |  Yes   |  Yes   |
 * |   D   |  Yes   |  Yes   |  Yes   |
 *
 * @param[in] leaf  Valid leaf handle. Must not be NULL.
 * @param[in] tier  Pressure tier [1, 4].
 * @return QX_TRUE if the leaf is evictable at the given tier.
 * @return QX_FALSE if leaf is NULL or not evictable at this tier.
 *
 * @note Thread-safe.
 */
QX_API QXBool qx_leaf_evictable_at_tier(
    QXLeafHandle leaf,
    QXTierId     tier
);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* QX_LEAF_H */
