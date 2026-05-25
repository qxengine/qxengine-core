// =============================================================================
// QXEngine Core – include/qxengine/manifest/qx_manifest.h
// Owner : Masa Bayu
// Created: 2026-05-24
// Repo   : https://github.com/qxengine/qxengine-core
// Description: C ABI for the QXManifest subsystem – parsing, validation,
//              and structured access to the application manifest JSON document.
//
// The manifest is the constitutional contract between the application and the
// QXEngine. It declares application identity, law compliance switches, memory
// budgets, ethics commitments, creativity policy, knowledge thresholds,
// economy limits, and vertical classifications.
//
// Schema:
//   URL    : https://qxengine.io/manifest/v1.0/schema.json
//   Version: 1.0.0
//
// Validation Rules (MFT-0001 – MFT-0014):
//   MFT-0001  schema field must equal the canonical schema URL.
//   MFT-0002  identity.appId must be non-blank and ≤ 128 chars.
//   MFT-0003  identity.owner must be non-blank.
//   MFT-0004  identity.platform must be "ios" or "android".
//   MFT-0005  all eight law compliance flags must be true.
//   MFT-0006  limit.memorySoftLimitMB ≥ QX_MIN_SOFT_LIMIT_MB (64).
//   MFT-0007  limit.memoryHardLimitMB ≥ QX_MIN_HARD_LIMIT_MB (128).
//   MFT-0008  hard limit must be > soft limit.
//   MFT-0009  segment budgetPercent values must sum to exactly 100.
//   MFT-0010  segment count must equal QX_SEGMENT_COUNT (9).
//   MFT-0011  all five ethics flags must be true.
//   MFT-0012  creativity.nativeFirstPolicy must be true.
//   MFT-0013  economy.networkBudgetMBPerSession must be > 0.
//   MFT-0014  economy.batteryDrainMaxPercentPer10Min ≤
//             QX_MAX_BATTERY_DRAIN_PCT (10.0).
//
// Thread Safety:
//   qx_manifest_parse() and qx_manifest_destroy() are NOT thread-safe.
//   All accessor functions are thread-safe after a successful parse.
//
// Repository: https://github.com/qxengine/qxengine-core
// =============================================================================

#ifndef QXENGINE_MANIFEST_QX_MANIFEST_H
#define QXENGINE_MANIFEST_QX_MANIFEST_H

#include "qxengine/core/qx_error.h"
#include "qxengine/manifest/qx_manifest_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Parse a manifest JSON string and produce a QXManifestHandle.
 *
 * Performs structural deserialisation and all MFT-0001 – MFT-0014
 * validation rules. On success the handle is ready for use.
 *
 * @param json_utf8     Null-terminated UTF-8 JSON string. Copied internally.
 * @param json_length   Byte length of @p json_utf8 excluding null terminator.
 *                      Pass 0 to have the function compute strlen internally.
 * @param out_manifest  On success, receives the new manifest handle.
 * @param out_result    Optional. Receives structured validation errors even
 *                      when the function returns an error code.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if json_utf8 or out_manifest is NULL.
 * @return QX_ERR_MANIFEST_PARSE if the JSON is malformed.
 * @return QX_ERR_MANIFEST_INVALID if any MFT rule is violated.
 * @return QX_ERR_MANIFEST_SCHEMA_MISMATCH if the schema URL does not match.
 * @return QX_ERR_INTERNAL on unexpected allocation failure.
 *
 * @note Not thread-safe.
 */
QX_API QXResult qx_manifest_parse(
    const char                  *json_utf8,
    uint32_t                     json_length,
    QXManifestHandle            *out_manifest,
    QXManifestValidationResult  *out_result
);

/**
 * @brief Validate an already-parsed manifest against all MFT rules.
 *
 * @param manifest    Non-NULL manifest handle.
 * @param out_result  Non-NULL. Receives structured validation results.
 * @return QX_OK if all rules pass.
 * @return QX_ERR_NULL_HANDLE if manifest or out_result is NULL.
 * @return QX_ERR_MANIFEST_INVALID if any MFT rule is violated.
 *
 * @note Thread-safe after successful qx_manifest_parse().
 */
QX_API QXResult qx_manifest_validate(
    QXManifestHandle             manifest,
    QXManifestValidationResult  *out_result
);

/**
 * @brief Destroy a QXManifestHandle and release all internal resources.
 *
 * @param manifest  Handle returned by qx_manifest_parse(). No-op if NULL.
 *
 * @note Not thread-safe.
 */
QX_API void qx_manifest_destroy(
    QXManifestHandle manifest
);

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Block Accessors
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Retrieve the parsed identity block.
 *
 * @param manifest      Non-NULL manifest handle.
 * @param out_identity  Non-NULL. Receives a copy of QXManifestIdentity.
 * @return QX_OK on success. QX_ERR_NULL_HANDLE if either argument is NULL.
 * @note Thread-safe.
 */
QX_API QXResult qx_manifest_identity(
    QXManifestHandle     manifest,
    QXManifestIdentity  *out_identity
);

/**
 * @brief Retrieve the parsed lawCompliance block.
 *
 * @param manifest       Non-NULL manifest handle.
 * @param out_compliance Non-NULL. Receives a copy of QXManifestLawCompliance.
 * @return QX_OK on success. QX_ERR_NULL_HANDLE if either argument is NULL.
 * @note Thread-safe.
 */
QX_API QXResult qx_manifest_law_compliance(
    QXManifestHandle          manifest,
    QXManifestLawCompliance  *out_compliance
);

/**
 * @brief Retrieve the parsed limit block (includes all segment budgets).
 *
 * @param manifest   Non-NULL manifest handle.
 * @param out_limit  Non-NULL. Receives a copy of QXManifestLimit.
 * @return QX_OK on success. QX_ERR_NULL_HANDLE if either argument is NULL.
 * @note Thread-safe.
 */
QX_API QXResult qx_manifest_limit(
    QXManifestHandle  manifest,
    QXManifestLimit  *out_limit
);

/**
 * @brief Retrieve the budget for a single segment by ID.
 *
 * @param manifest     Non-NULL manifest handle.
 * @param segment_id   Null-terminated segment ID (e.g. "QLM-IMG").
 * @param out_segment  Non-NULL. Receives a copy of QXManifestSegment.
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if manifest or out_segment is NULL.
 * @return QX_ERR_UNKNOWN_SEGMENT if segment_id is not found.
 * @note Thread-safe.
 */
QX_API QXResult qx_manifest_segment(
    QXManifestHandle    manifest,
    const char         *segment_id,
    QXManifestSegment  *out_segment
);

/**
 * @brief Retrieve the parsed ethics block.
 *
 * @param manifest    Non-NULL manifest handle.
 * @param out_ethics  Non-NULL. Receives a copy of QXManifestEthics.
 * @return QX_OK on success. QX_ERR_NULL_HANDLE if either argument is NULL.
 * @note Thread-safe.
 */
QX_API QXResult qx_manifest_ethics(
    QXManifestHandle   manifest,
    QXManifestEthics  *out_ethics
);

/**
 * @brief Retrieve the parsed creativity block.
 *
 * @param manifest        Non-NULL manifest handle.
 * @param out_creativity  Non-NULL. Receives a copy of QXManifestCreativity.
 * @return QX_OK on success. QX_ERR_NULL_HANDLE if either argument is NULL.
 * @note Thread-safe.
 */
QX_API QXResult qx_manifest_creativity(
    QXManifestHandle      manifest,
    QXManifestCreativity *out_creativity
);

/**
 * @brief Retrieve the parsed knowledge block.
 *
 * @param manifest       Non-NULL manifest handle.
 * @param out_knowledge  Non-NULL. Receives a copy of QXManifestKnowledge.
 * @return QX_OK on success. QX_ERR_NULL_HANDLE if either argument is NULL.
 * @note Thread-safe.
 */
QX_API QXResult qx_manifest_knowledge(
    QXManifestHandle     manifest,
    QXManifestKnowledge *out_knowledge
);

/**
 * @brief Retrieve the parsed economy block.
 *
 * @param manifest     Non-NULL manifest handle.
 * @param out_economy  Non-NULL. Receives a copy of QXManifestEconomy.
 * @return QX_OK on success. QX_ERR_NULL_HANDLE if either argument is NULL.
 * @note Thread-safe.
 */
QX_API QXResult qx_manifest_economy(
    QXManifestHandle   manifest,
    QXManifestEconomy *out_economy
);

/**
 * @brief Retrieve the parsed verticals block.
 *
 * @param manifest       Non-NULL manifest handle.
 * @param out_verticals  Non-NULL. Receives a copy of QXManifestVerticals.
 * @return QX_OK on success. QX_ERR_NULL_HANDLE if either argument is NULL.
 * @note Thread-safe.
 */
QX_API QXResult qx_manifest_verticals(
    QXManifestHandle     manifest,
    QXManifestVerticals *out_verticals
);

/**
 * @brief Retrieve the parsed meta block.
 *
 * @param manifest  Non-NULL manifest handle.
 * @param out_meta  Non-NULL. Receives a copy of QXManifestMeta.
 * @return QX_OK on success. QX_ERR_NULL_HANDLE if either argument is NULL.
 * @note Thread-safe.
 */
QX_API QXResult qx_manifest_meta(
    QXManifestHandle  manifest,
    QXManifestMeta   *out_meta
);

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Scalar Accessors
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Return the application ID string from the manifest.
 *
 * @param manifest  Non-NULL manifest handle.
 * @param buffer    Caller-allocated buffer ≥ QX_MANIFEST_APP_ID_MAX bytes.
 * @return QX_OK on success. QX_ERR_NULL_HANDLE if either argument is NULL.
 * @note Thread-safe.
 */
QX_API QXResult qx_manifest_app_id(
    QXManifestHandle  manifest,
    char             *buffer
);

/**
 * @brief Return the target platform string ("ios" or "android").
 *
 * @param manifest  Non-NULL manifest handle.
 * @param buffer    Caller-allocated buffer ≥ QX_MANIFEST_PLATFORM_MAX bytes.
 * @return QX_OK on success. QX_ERR_NULL_HANDLE if either argument is NULL.
 * @note Thread-safe.
 */
QX_API QXResult qx_manifest_platform(
    QXManifestHandle  manifest,
    char             *buffer
);

/**
 * @brief Return the aggregate soft memory limit in megabytes.
 *
 * @param manifest  Non-NULL manifest handle.
 * @param out_mb    Receives the soft limit (≥ QX_MIN_SOFT_LIMIT_MB).
 * @return QX_OK on success. QX_ERR_NULL_HANDLE if either argument is NULL.
 * @note Thread-safe.
 */
QX_API QXResult qx_manifest_soft_limit_mb(
    QXManifestHandle  manifest,
    double           *out_mb
);

/**
 * @brief Return the aggregate hard memory limit in megabytes.
 *
 * @param manifest  Non-NULL manifest handle.
 * @param out_mb    Receives the hard limit (≥ QX_MIN_HARD_LIMIT_MB).
 * @return QX_OK on success. QX_ERR_NULL_HANDLE if either argument is NULL.
 * @note Thread-safe.
 */
QX_API QXResult qx_manifest_hard_limit_mb(
    QXManifestHandle  manifest,
    double           *out_mb
);

/**
 * @brief Return QX_TRUE if all eight constitutional law flags are active.
 *
 * @param manifest        Non-NULL manifest handle.
 * @param out_all_active  Receives QX_TRUE or QX_FALSE.
 * @return QX_OK on success. QX_ERR_NULL_HANDLE if either argument is NULL.
 * @note Thread-safe.
 */
QX_API QXResult qx_manifest_all_laws_active(
    QXManifestHandle  manifest,
    QXBool           *out_all_active
);

/**
 * @brief Return QX_TRUE if all five ethics flags are active.
 *
 * @param manifest          Non-NULL manifest handle.
 * @param out_fully_ethical Receives QX_TRUE or QX_FALSE.
 * @return QX_OK on success. QX_ERR_NULL_HANDLE if either argument is NULL.
 * @note Thread-safe.
 */
QX_API QXResult qx_manifest_is_fully_ethical(
    QXManifestHandle  manifest,
    QXBool           *out_fully_ethical
);

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Serialisation
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Serialise the manifest back to a compact JSON string.
 *
 * @param manifest     Non-NULL manifest handle.
 * @param buffer       Caller-allocated output buffer.
 * @param buffer_size  Size of @p buffer in bytes.
 * @param out_written  Optional. Receives bytes written (excluding null).
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if manifest or buffer is NULL.
 * @return QX_ERR_BUFFER_TOO_SMALL if buffer_size is insufficient;
 *         *out_written receives the required size.
 * @note Thread-safe.
 */
QX_API QXResult qx_manifest_to_json(
    QXManifestHandle  manifest,
    char             *buffer,
    uint32_t          buffer_size,
    uint32_t         *out_written
);

/**
 * @brief Return the byte size required to serialise the manifest to JSON.
 *
 * @param manifest       Non-NULL manifest handle.
 * @param out_size_bytes Receives the required buffer size including null.
 * @return QX_OK on success. QX_ERR_NULL_HANDLE if either argument is NULL.
 * @note Thread-safe.
 */
QX_API QXResult qx_manifest_json_size(
    QXManifestHandle  manifest,
    uint32_t         *out_size_bytes
);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* QXENGINE_MANIFEST_QX_MANIFEST_H */
