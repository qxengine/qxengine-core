/* ============================================================================
 * QXEngine Core – qx_types.h
 * Owner:      Masa Bayu
 * Created:    2026-05-24
 * Description: Primitive type definitions for the QXEngine C ABI boundary.
 *              All types used across the public API are declared here.
 *              Rules:
 *                - No C++ types cross the ABI boundary
 *                - All sizes are explicit (uint8_t, uint32_t, uint64_t)
 *                - No platform-specific assumptions
 *                - No typedefs that hide pointer semantics except handles
 *              This header is safe to include from C, C++, Swift, and Kotlin.
 * Repository: https://github.com/qxengine/qxengine-core
 * ============================================================================
 */

#ifndef QX_TYPES_H
#define QX_TYPES_H

/* --------------------------------------------------------------------------
 * Standard integer types
 * Required: C99 or later, C++11 or later
 * -------------------------------------------------------------------------- */
#include <stdint.h>
#include <stddef.h>

/* --------------------------------------------------------------------------
 * C ABI boundary guard
 * -------------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * MARK: – Primitive types
 * ============================================================================ */

/**
 * @brief Boolean type for the C ABI boundary.
 *
 * Using uint8_t instead of C99 _Bool for maximum ABI compatibility
 * across C, C++, Swift (bridged as UInt8), and Kotlin JNI (jbyte).
 */
typedef uint8_t QXBool;

/** @brief True value for QXBool. */
#define QX_TRUE  1u

/** @brief False value for QXBool. */
#define QX_FALSE 0u

/**
 * @brief Byte size type.
 *
 * Used for all memory size values throughout the API.
 * 64-bit unsigned to support devices with more than 4 GB RAM.
 */
typedef uint64_t QXSize;

/**
 * @brief Snapshot sequence number.
 *
 * Monotonically increasing counter per engine session.
 * Wraps at UINT64_MAX (effectively never in practice).
 */
typedef uint64_t QXSequence;

/**
 * @brief Result code type.
 *
 * Every QXEngine API function that can fail returns a QXResult.
 * QX_OK (0) indicates success.
 * Negative values indicate specific error conditions.
 * Positive values are reserved for future use.
 *
 * @see qx_error.h for the full error code registry.
 */
typedef int32_t QXResult;

/* ============================================================================
 * MARK: – Enumerated ID types
 * All enums use fixed-width integer backing for ABI stability.
 * ============================================================================ */

/**
 * @brief Leaf class identifier.
 *
 * Determines eviction eligibility and priority.
 *
 * | Value | Class | Name      | Evictable | Min Pressure Tier |
 * |-------|-------|-----------|-----------|-------------------|
 * |   0   |   A   | Protected | Never     | –                 |
 * |   1   |   B   | Essential | Yes       | Tier 4            |
 * |   2   |   C   | Standard  | Yes       | Tier 3            |
 * |   3   |   D   | Transient | Yes       | Tier 2            |
 */
typedef uint8_t QXLeafClassId;

#define QX_LEAF_CLASS_A 0u   /**< Protected – never evicted  */
#define QX_LEAF_CLASS_B 1u   /**< Essential – Tier 4+        */
#define QX_LEAF_CLASS_C 2u   /**< Standard  – Tier 3+        */
#define QX_LEAF_CLASS_D 3u   /**< Transient – Tier 2+        */

/**
 * @brief Constitutional law identifier.
 *
 * Maps to the eight constitutional laws of QXEngine.
 * Values are 1-indexed to match the law numbering in the spec.
 */
typedef uint8_t QXLawId;

#define QX_LAW_PATTERN     1u   /**< Law 1 – Pattern     */
#define QX_LAW_LIMIT       2u   /**< Law 2 – Limit       */
#define QX_LAW_PAIRS       3u   /**< Law 3 – Pairs       */
#define QX_LAW_EQUILIBRIUM 4u   /**< Law 4 – Equilibrium */
#define QX_LAW_KNOWLEDGE   5u   /**< Law 5 – Knowledge   */
#define QX_LAW_ETHICS      6u   /**< Law 6 – Ethics      */
#define QX_LAW_CREATIVITY  7u   /**< Law 7 – Creativity  */
#define QX_LAW_ECONOMY     8u   /**< Law 8 – Economy     */

/**
 * @brief Violation severity identifier.
 *
 * | Value | Name     | Action required              |
 * |-------|----------|------------------------------|
 * |   0   | Info     | Log only                     |
 * |   1   | Warning  | Monitor and plan             |
 * |   2   | Critical | Immediate corrective action  |
 * |   3   | Fatal    | Engine cannot certify app    |
 */
typedef uint8_t QXSeverityId;

#define QX_SEVERITY_INFO     0u
#define QX_SEVERITY_WARNING  1u
#define QX_SEVERITY_CRITICAL 2u
#define QX_SEVERITY_FATAL    3u

/**
 * @brief Memory pressure tier identifier.
 *
 * | Value | Name     | Soft utilisation | Eviction target       |
 * |-------|----------|------------------|-----------------------|
 * |   1   | Normal   | < 70%            | None                  |
 * |   2   | Elevated | ≥ 70%            | Class D               |
 * |   3   | High     | ≥ 85%            | Class D + C           |
 * |   4   | Critical | ≥ 95%            | Class D + C + B       |
 */
typedef uint8_t QXTierId;

#define QX_TIER_NORMAL   1u
#define QX_TIER_ELEVATED 2u
#define QX_TIER_HIGH     3u
#define QX_TIER_CRITICAL 4u

/**
 * @brief Certification tier identifier.
 *
 * | Value | Name         | Min health score |
 * |-------|--------------|------------------|
 * |   0   | Invalid      | –                |
 * |   1   | Standard     | 60%              |
 * |   2   | Professional | 80%              |
 * |   3   | Sovereign    | 95%              |
 */
typedef uint8_t QXCertificationId;

#define QX_CERT_INVALID      0u
#define QX_CERT_STANDARD     1u
#define QX_CERT_PROFESSIONAL 2u
#define QX_CERT_SOVEREIGN    3u

/* ============================================================================
 * MARK: – Opaque handles
 * Internal C++ objects are exposed as typed opaque pointers.
 * Callers never see struct internals.
 * Every handle created by a qx_*_create() function must be
 * released by the corresponding qx_*_destroy() function.
 * ============================================================================ */

/** @brief Opaque handle to a QXLeaf allocation unit. */
typedef struct QXLeaf_s*                QXLeafHandle;

/** @brief Opaque handle to a QXSegment memory region. */
typedef struct QXSegment_s*             QXSegmentHandle;

/** @brief Opaque handle to the QXMemloc allocation engine. */
typedef struct QXMemloc_s*              QXMemlocHandle;

/** @brief Opaque handle to the QXPressureCoordinator. */
typedef struct QXPressureCoordinator_s*  QXPressureHandle;

/** Alias used by qx_pressure.h for the pressure coordinator handle. */
typedef QXPressureHandle                QXPressureCoordinatorHandle;

/** @brief Opaque handle to the QXLawEnforcer. */
typedef struct QXLawEnforcer_s*         QXLawEnforcerHandle;

/** @brief Opaque handle to a QXCognitiveSnapshot. */
typedef struct QXSnapshot_s*            QXSnapshotHandle;

/** @brief Opaque handle to a parsed QXManifest. */
typedef struct QXManifest_s*            QXManifestHandle;

/** @brief Opaque handle to the QXTelemetry log. */
typedef struct QXTelemetry_s*           QXTelemetryHandle;

/** @brief Opaque handle to a top-level QXEngine instance. */
typedef struct QXEngine_s*              QXEngineHandle;

/* ============================================================================
 * MARK: – String buffer type
 * Used for API functions that write string output to caller-owned buffers.
 * ============================================================================ */

/**
 * @brief Caller-owned string output buffer.
 *
 * Used by API functions that return variable-length strings.
 * The caller allocates [buffer] of [capacity] bytes.
 * The engine writes the null-terminated string and sets [length]
 * to the number of bytes written (excluding the null terminator).
 *
 * If [capacity] is too small, the function returns QX_ERR_BUFFER_TOO_SMALL
 * and sets [length] to the required capacity (including null terminator).
 */
typedef struct QXStringBuffer {
    char*    buffer;    /**< Caller-allocated output buffer      */
    uint32_t capacity;  /**< Capacity of buffer in bytes         */
    uint32_t length;    /**< Bytes written (excl. null terminator) */
} QXStringBuffer;

/* ============================================================================
 * MARK: – Timestamp type
 * Unix epoch milliseconds. Consistent across all platforms.
 * ============================================================================ */

/**
 * @brief Milliseconds since Unix epoch (1970-01-01T00:00:00Z).
 *
 * Used for all timestamps in the QXEngine C ABI.
 * Platform adapters convert OS-specific time types to this format.
 */
typedef uint64_t QXTimestamp;

/* ============================================================================
 * MARK: – Utility macros
 * ============================================================================ */

/** @brief Returns the number of elements in a fixed-size C array. */
#define QX_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/** @brief Marks a parameter as intentionally unused (suppresses warnings). */
#define QX_UNUSED(x) ((void)(x))

/** @brief Marks a function as part of the public C ABI. */
#if defined(_WIN32) || defined(__CYGWIN__)
    #define QX_API __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
    #define QX_API __attribute__((visibility("default")))
#else
    #define QX_API
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* QX_TYPES_H */
