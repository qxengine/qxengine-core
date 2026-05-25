/* ============================================================================
 * QXEngine Core – qx_error.h
 * Owner:      Masa Bayu
 * Created:    2026-05-24
 * Description: Complete error code registry for the QXEngine C ABI.
 *              Every function that can fail returns a QXResult (int32_t).
 *              QX_OK (0) always means success.
 *              Negative values are error codes defined in this file.
 *              Positive values are reserved for future use.
 *
 *              Error code ranges:
 *                0           Success
 *                -1000 series  General / null handle errors
 *                -1000 series  Manifest errors
 *                -2000 series  Memory errors
 *                -3000 series  Constitutional law errors
 *                -4000 series  Engine lifecycle errors
 *                -5000 series  Telemetry errors
 *                -6000 series  Security errors
 *                -9999         Internal / unexpected errors
 *
 *              Usage:
 *                QXResult result = qx_engine_allocate(...);
 *                if (result != QX_OK) {
 *                    const char* msg = qx_error_string(result);
 *                    // handle error
 *                }
 *
 * Repository: https://github.com/qxengine/qxengine-core
 * ============================================================================
 */

#ifndef QX_ERROR_H
#define QX_ERROR_H

#include <qxengine/core/qx_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * MARK: – Success
 * ============================================================================ */

/** @brief Operation completed successfully. */
#define QX_OK                               0

/* ============================================================================
 * MARK: – General errors (-1 to -999)
 * ============================================================================ */

/**
 * @brief A required pointer argument was NULL.
 *
 * Returned when any required pointer parameter (handle, out-parameter,
 * string pointer) is NULL. The operation was not performed.
 */
#define QX_ERR_NULL_HANDLE                  -1

/** Alias for a required pointer argument that was NULL. */
#define QX_ERR_NULL_ARG                     QX_ERR_NULL_HANDLE

/**
 * @brief Output buffer is too small to hold the result.
 *
 * Returned by string-output functions when the provided QXStringBuffer
 * capacity is insufficient. The required capacity is written to
 * QXStringBuffer.length.
 */
#define QX_ERR_BUFFER_TOO_SMALL             -2

/**
 * @brief The requested feature is not supported on this platform.
 */
#define QX_ERR_NOT_SUPPORTED                -3

/**
 * @brief An argument value is outside the valid range.
 */
#define QX_ERR_INVALID_ARGUMENT             -4

/* ============================================================================
 * MARK: – Manifest errors (-1000 to -1099)
 * ============================================================================ */

/**
 * @brief Manifest file was not found at the specified path.
 */
#define QX_ERR_MANIFEST_NOT_FOUND           -1001

/**
 * @brief Manifest file could not be read (permissions, IO error).
 */
#define QX_ERR_MANIFEST_UNREADABLE          -1002

/**
 * @brief Manifest JSON is syntactically malformed.
 *
 * The JSON parser encountered an unexpected token or structure.
 * Use qx_error_detail() to retrieve the parser error message.
 */
#define QX_ERR_MANIFEST_PARSE               -1003

/**
 * @brief Manifest JSON decoded successfully but failed constitutional
 *        validation rules.
 *
 * One or more fields violate the constitutional requirements.
 * Use qx_manifest_validation_errors() to retrieve the full list.
 */
#define QX_ERR_MANIFEST_INVALID             -1004

/**
 * @brief Manifest $schema URL does not match the canonical schema URL.
 *
 * Expected: https://qxengine.io/manifest/v1.0/schema.json
 */
#define QX_ERR_MANIFEST_SCHEMA_MISMATCH     -1005

/**
 * @brief Manifest identity.applicationId is blank or missing a dot.
 */
#define QX_ERR_MANIFEST_INVALID_APP_ID      -1006

/**
 * @brief Manifest segment budget percentages do not sum to 100%.
 */
#define QX_ERR_MANIFEST_BUDGET_SUM          -1007

/**
 * @brief Manifest declares fewer than QX_SEGMENT_COUNT segments.
 */
#define QX_ERR_MANIFEST_SEGMENT_COUNT       -1008

/**
 * @brief Manifest memory limits are below constitutional minimums.
 *
 * softLimitMB must be ≥ QX_MIN_SOFT_LIMIT_MB (64 MB).
 * hardLimitMB must be ≥ QX_MIN_HARD_LIMIT_MB (128 MB).
 */
#define QX_ERR_MANIFEST_LIMIT_TOO_LOW       -1009

/**
 * @brief One or more constitutional law compliance flags are false.
 *
 * All eight law flags must be true in lawCompliance.
 */
#define QX_ERR_MANIFEST_LAW_INACTIVE        -1010

/* ============================================================================
 * MARK: – Memory errors (-2000 to -2099)
 * ============================================================================ */

/**
 * @brief Allocation label is blank or whitespace only.
 *
 * Law 1 (Pattern): every allocation must carry a non-blank label.
 */
#define QX_ERR_LABEL_BLANK                  -2001

/**
 * @brief Allocation label is shorter than QX_LABEL_MIN_LENGTH (3 bytes).
 *
 * Law 1 (Pattern): label must be at least 3 UTF-8 bytes.
 */
#define QX_ERR_LABEL_TOO_SHORT              -2002

/**
 * @brief Allocation label is longer than QX_LABEL_MAX_LENGTH (128 bytes).
 *
 * Law 1 (Pattern): label must not exceed 128 UTF-8 bytes.
 */
#define QX_ERR_LABEL_TOO_LONG               -2003

/**
 * @brief The specified segment ID is not one of the nine canonical segments.
 *
 * Valid segment IDs: QLM-CORE, QLM-UI, QLM-DATA, QLM-IMG, QLM-NET,
 *                   QLM-AI, QLM-SEC, QLM-LOG, QLM-TEMP
 */
#define QX_ERR_UNKNOWN_SEGMENT              -2004

/**
 * @brief The segment has no available QLM slots.
 *
 * Law 2 (Limit): the segment slot count has reached its maximum.
 * Evict existing leaves before allocating new ones.
 */
#define QX_ERR_NO_SLOT                      -2005

/**
 * @brief Allocation would exceed the segment soft memory limit.
 *
 * Law 2 (Limit): soft budget exceeded. The engine attempted eviction
 * but could not reclaim sufficient memory.
 */
#define QX_ERR_SOFT_BUDGET                  -2006

/**
 * @brief Allocation would exceed the segment hard memory limit.
 *
 * Law 2 (Limit): hard budget exceeded. This is a fatal constraint.
 * No allocation is possible until leaves are evicted or deallocated.
 */
#define QX_ERR_HARD_BUDGET                  -2007

/**
 * @brief The specified leaf ID was not found in any segment.
 *
 * The leaf may have already been evicted or deallocated.
 */
#define QX_ERR_LEAF_NOT_FOUND               -2008

/**
 * @brief A deallocation was attempted on an already-released leaf.
 *
 * Law 3 (Pairs): double-free is a constitutional violation.
 * Every allocation must be deallocated exactly once.
 */
#define QX_ERR_DOUBLE_FREE                  -2009

/**
 * @brief An eviction was attempted on a Class A (Protected) leaf.
 *
 * Law 2 (Limit): Class A leaves are protected from eviction at all tiers.
 * They can only be removed via explicit deallocation.
 */
#define QX_ERR_PROTECTED_EVICTION           -2010

/**
 * @brief Eviction cascade did not reclaim sufficient memory.
 *
 * The pressure coordinator attempted eviction but the reclaimed bytes
 * were insufficient to satisfy the allocation request.
 */
#define QX_ERR_INSUFFICIENT_EVICTION        -2011

/**
 * @brief The leaf class ID is not a valid QXLeafClassId value.
 *
 * Valid values: QX_LEAF_CLASS_A (0), QX_LEAF_CLASS_B (1),
 *               QX_LEAF_CLASS_C (2), QX_LEAF_CLASS_D (3)
 */
#define QX_ERR_INVALID_LEAF_CLASS           -2012

/* ============================================================================
 * MARK: – Constitutional law errors (-3000 to -3099)
 * ============================================================================ */

/**
 * @brief Law 1 (Pattern) violation detected.
 *
 * One or more allocations were made without a valid label.
 */
#define QX_ERR_LAW_PATTERN                  -3001

/**
 * @brief Law 2 (Limit) violation detected.
 *
 * One or more segments have exceeded their hard memory budget.
 */
#define QX_ERR_LAW_LIMIT                    -3002

/**
 * @brief Law 3 (Pairs) violation detected.
 *
 * Deallocation ratio is below the minimum threshold or orphaned
 * bytes exceed the critical threshold.
 */
#define QX_ERR_LAW_PAIRS                    -3003

/**
 * @brief Law 4 (Equilibrium) violation detected.
 *
 * Memory distribution across segments is severely imbalanced.
 */
#define QX_ERR_LAW_EQUILIBRIUM              -3004

/**
 * @brief Law 5 (Knowledge) violation detected.
 *
 * The knowledge score has fallen below QX_MIN_KNOWLEDGE_SCORE.
 * Mandatory events are not being logged.
 */
#define QX_ERR_LAW_KNOWLEDGE                -3005

/**
 * @brief Law 6 (Ethics) violation detected.
 *
 * One or more ethics flags are false in the manifest.
 * This is a fatal violation that prevents SOVEREIGN certification.
 */
#define QX_ERR_LAW_ETHICS                   -3006

/**
 * @brief Law 7 (Creativity) violation detected.
 *
 * nativeFirstPolicy is false or native utilisation is below target.
 */
#define QX_ERR_LAW_CREATIVITY               -3007

/**
 * @brief Law 8 (Economy) violation detected.
 *
 * Network redundancy or battery drain exceeds constitutional limits.
 */
#define QX_ERR_LAW_ECONOMY                  -3008

/* ============================================================================
 * MARK: – Engine lifecycle errors (-4000 to -4099)
 * ============================================================================ */

/**
 * @brief Operation requires the engine to be running but it is not.
 *
 * Call qx_engine_start() before performing memory or evaluation operations.
 */
#define QX_ERR_NOT_READY                    -4001

/**
 * @brief qx_engine_start() was called on an already-running engine.
 */
#define QX_ERR_ALREADY_RUNNING              -4002

/**
 * @brief Operation was attempted on a stopped engine.
 *
 * A stopped engine cannot be restarted. Create a new engine instance.
 */
#define QX_ERR_ALREADY_STOPPED              -4003

/**
 * @brief Engine configuration is missing or incomplete.
 *
 * Call qx_engine_create() with a valid QXEngineConfig before starting.
 */
#define QX_ERR_NOT_CONFIGURED               -4004

/* ============================================================================
 * MARK: – Telemetry errors (-5000 to -5099)
 * ============================================================================ */

/**
 * @brief Telemetry write operation failed.
 */
#define QX_ERR_TELEMETRY_WRITE              -5001

/**
 * @brief Audit report generation failed.
 */
#define QX_ERR_TELEMETRY_REPORT             -5002

/**
 * @brief Knowledge score has fallen below the minimum threshold.
 *
 * Law 5 (Knowledge): mandatory events are not being recorded.
 */
#define QX_ERR_KNOWLEDGE_SCORE_LOW          -5003

/* ============================================================================
 * MARK: – Security errors (-6000 to -6099)
 * ============================================================================ */

/**
 * @brief Manifest integrity check failed.
 *
 * The SHA-256 digest of the current manifest does not match
 * the stored integrity record.
 */
#define QX_ERR_INTEGRITY_MISMATCH           -6001

/**
 * @brief Encryption operation failed.
 */
#define QX_ERR_ENCRYPTION_FAILED            -6002

/**
 * @brief Decryption operation failed.
 */
#define QX_ERR_DECRYPTION_FAILED            -6003

/* ============================================================================
 * MARK: – Internal error (-9999)
 * ============================================================================ */

/**
 * @brief An unexpected internal error occurred.
 *
 * This should never happen in production. If encountered, file a bug
 * report at https://github.com/qxengine/qxengine-core/issues
 * with the full error detail string from qx_error_detail().
 */
#define QX_ERR_INTERNAL                     -9999

/* ============================================================================
 * MARK: – Legacy QX_ERROR_* aliases (implementation sources)
 * ============================================================================ */

#define QX_ERROR_INVALID_ARGUMENT           QX_ERR_INVALID_ARGUMENT
#define QX_ERROR_BUFFER_TOO_SMALL           QX_ERR_BUFFER_TOO_SMALL
#define QX_ERROR_INTERNAL                   QX_ERR_INTERNAL
#define QX_ERROR_MANIFEST_MISSING           QX_ERR_MANIFEST_NOT_FOUND
#define QX_ERROR_MANIFEST_INVALID           QX_ERR_MANIFEST_INVALID
#define QX_ERROR_ENGINE_NOT_INITIALISED     QX_ERR_NOT_READY
#define QX_ERROR_OUT_OF_MEMORY              QX_ERR_INTERNAL
#define QX_ERROR_OUT_OF_RANGE               QX_ERR_INVALID_ARGUMENT
#define QX_ERROR_NOT_FOUND                  QX_ERR_LEAF_NOT_FOUND
#define QX_ERROR_LAW_VIOLATION              QX_ERR_LAW_PATTERN

/* ============================================================================
 * MARK: – Error utility functions
 * ============================================================================ */

/**
 * @brief Returns a human-readable description of a QXResult error code.
 *
 * The returned string is statically allocated and must not be freed
 * or modified by the caller.
 *
 * For QX_OK, returns "Success".
 * For unknown codes, returns "Unknown error code".
 *
 * @param[in] result A QXResult error code.
 * @return Null-terminated UTF-8 string describing the error.
 *
 * @note Thread-safe. Returns a pointer to static storage.
 */
QX_API const char* qx_error_string(QXResult result);

/**
 * @brief Returns a detailed diagnostic message for the last error
 *        that occurred on the calling thread.
 *
 * Provides additional context beyond qx_error_string(), such as
 * the specific JSON parse error or the segment name that caused a
 * budget violation.
 *
 * The returned string is valid until the next QXEngine API call
 * on the calling thread. Must not be freed by the caller.
 *
 * Returns an empty string if no error has occurred on this thread.
 *
 * @return Null-terminated UTF-8 string with diagnostic detail.
 *
 * @note Thread-safe. Uses thread-local storage.
 */
QX_API const char* qx_error_detail(void);

/**
 * @brief Returns QX_TRUE if the result code indicates success.
 *
 * Equivalent to: result == QX_OK
 *
 * @param[in] result A QXResult value.
 * @return QX_TRUE if result == QX_OK, QX_FALSE otherwise.
 *
 * @note Thread-safe.
 */
static inline QXBool qx_ok(QXResult result) {
    return result == 0 ? 1u : 0u;
}

/**
 * @brief Returns QX_TRUE if the result code indicates failure.
 *
 * Equivalent to: result != QX_OK
 *
 * @param[in] result A QXResult value.
 * @return QX_TRUE if result != QX_OK, QX_FALSE otherwise.
 *
 * @note Thread-safe.
 */
static inline QXBool qx_failed(QXResult result) {
    return result != 0 ? 1u : 0u;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* QX_ERROR_H */
