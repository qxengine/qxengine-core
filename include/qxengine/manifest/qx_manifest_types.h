// =============================================================================
// QXEngine Core – include/qxengine/manifest/qx_manifest_types.h
// Owner : Masa Bayu
// Created: 2026-05-24
// Repo   : https://github.com/qxengine/qxengine-core
// Description: Types layer – field size constants and all manifest data
//              structures consumed by the manifest subsystem, the engine,
//              and platform adapters.
// =============================================================================

#ifndef QXENGINE_MANIFEST_QX_MANIFEST_TYPES_H
#define QXENGINE_MANIFEST_QX_MANIFEST_TYPES_H

#include "qxengine/core/qx_types.h"
#include "qxengine/core/qx_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Field Size Constants
// ─────────────────────────────────────────────────────────────────────────────

#define QX_MANIFEST_APP_ID_MAX          129u  /**< appId buffer (128 + null).      */
#define QX_MANIFEST_APP_NAME_MAX        129u  /**< appName buffer.                 */
#define QX_MANIFEST_OWNER_MAX           129u  /**< owner buffer.                   */
#define QX_MANIFEST_VERSION_MAX          33u  /**< version buffer.                 */
#define QX_MANIFEST_DATE_MAX             33u  /**< ISO-8601 date buffer.           */
#define QX_MANIFEST_PLATFORM_MAX         16u  /**< "ios" | "android" + null.       */
#define QX_MANIFEST_ENGINE_VER_MAX       33u  /**< minimumEngineVersion buffer.    */
#define QX_MANIFEST_SCHEMA_URL_MAX      129u  /**< schema URL buffer.              */
#define QX_MANIFEST_SEGMENT_ID_MAX       16u  /**< segment ID buffer.              */
#define QX_MANIFEST_SEGMENT_NAME_MAX     65u  /**< segment name buffer.            */
#define QX_MANIFEST_COMMERCE_MAX         33u  /**< commerceModel buffer.           */
#define QX_MANIFEST_VERTICAL_MAX         65u  /**< vertical name buffer.           */
#define QX_MANIFEST_VERTICAL_SEC_MAX      8u  /**< max secondary verticals.        */
#define QX_MANIFEST_CAPABILITY_ID_MAX    65u  /**< capability ID buffer.           */
#define QX_MANIFEST_CAPABILITY_CAP       32u  /**< max declared capabilities.      */
#define QX_MANIFEST_GENERATED_BY_MAX     65u  /**< meta.generatedBy buffer.        */
#define QX_MANIFEST_SPEC_VER_MAX         33u  /**< meta.specVersion buffer.        */
#define QX_MANIFEST_MAX_ERRORS           32u  /**< max validation errors per call. */

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXManifestIdentity
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Parsed identity block from the manifest.
 *
 * Corresponds to the JSON key "identity".
 */
typedef struct QXManifestIdentity {
    /** Application bundle / package identifier. Non-blank, max 128 chars. */
    char    app_id[QX_MANIFEST_APP_ID_MAX];

    /** Human-readable application name. Non-blank. */
    char    app_name[QX_MANIFEST_APP_NAME_MAX];

    /** Registered owner or organisation name. Non-blank. */
    char    owner[QX_MANIFEST_OWNER_MAX];

    /** Application version string (semver recommended). */
    char    version[QX_MANIFEST_VERSION_MAX];

    /** ISO-8601 creation date of this manifest document. */
    char    created[QX_MANIFEST_DATE_MAX];

    /**
     * @brief Target platform. Must be "ios" or "android".
     *
     * Validated by MFT-0004. Determines which platform adapter is activated.
     */
    char    platform[QX_MANIFEST_PLATFORM_MAX];

    /**
     * @brief Minimum QXEngine version required by this manifest.
     *
     * Format: "MAJOR.MINOR.PATCH". Engine rejects manifests requiring a
     * higher version than QX_ENGINE_VERSION.
     */
    char    minimum_engine_version[QX_MANIFEST_ENGINE_VER_MAX];
} QXManifestIdentity;

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXManifestLawCompliance
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Parsed lawCompliance block from the manifest.
 *
 * All eight flags must be QX_TRUE (validated by MFT-0005).
 */
typedef struct QXManifestLawCompliance {
    QXBool  law1_pattern;       /**< Law 1 – Pattern active.      */
    QXBool  law2_limit;         /**< Law 2 – Limit active.        */
    QXBool  law3_pairs;         /**< Law 3 – Pairs active.        */
    QXBool  law4_equilibrium;   /**< Law 4 – Equilibrium active.  */
    QXBool  law5_knowledge;     /**< Law 5 – Knowledge active.    */
    QXBool  law6_ethics;        /**< Law 6 – Ethics active.       */
    QXBool  law7_creativity;    /**< Law 7 – Creativity active.   */
    QXBool  law8_economy;       /**< Law 8 – Economy active.      */

    /**
     * @brief QX_TRUE if and only if all eight law flags are QX_TRUE.
     *
     * Computed by qx_manifest_parse(); not read from JSON.
     */
    QXBool  all_laws_active;
} QXManifestLawCompliance;

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXManifestSegment
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Budget declaration for a single QLM memory segment.
 *
 * Corresponds to one entry in the JSON array "limit.segments".
 */
typedef struct QXManifestSegment {
    /** Segment identifier. Must match one of the QX_SEGMENT_ID_* constants. */
    char        id[QX_MANIFEST_SEGMENT_ID_MAX];

    /** Human-readable segment name (e.g. "Core Engine"). */
    char        name[QX_MANIFEST_SEGMENT_NAME_MAX];

    /**
     * @brief Percentage of total memory budget allocated to this segment.
     *
     * All segment values must sum to exactly 100 (MFT-0009).
     */
    double      budget_percent;

    /** Soft memory limit for this segment in megabytes. Must be positive. */
    double      soft_limit_mb;

    /**
     * @brief Hard memory limit for this segment in megabytes.
     *
     * Must be strictly greater than soft_limit_mb.
     */
    double      hard_limit_mb;
} QXManifestSegment;

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXManifestLimit
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Parsed limit block from the manifest.
 *
 * Corresponds to the JSON key "limit".
 */
typedef struct QXManifestLimit {
    /**
     * @brief Aggregate soft memory limit in megabytes.
     *
     * Must be ≥ QX_MIN_SOFT_LIMIT_MB (64). Validated by MFT-0006.
     */
    double              memory_soft_limit_mb;

    /**
     * @brief Aggregate hard memory limit in megabytes.
     *
     * Must be ≥ QX_MIN_HARD_LIMIT_MB (128) and > memory_soft_limit_mb.
     * Validated by MFT-0007 and MFT-0008.
     */
    double              memory_hard_limit_mb;

    /**
     * @brief Per-segment budget declarations.
     *
     * Must contain exactly QX_SEGMENT_COUNT (9) entries (MFT-0010).
     */
    QXManifestSegment   segments[QX_SEGMENT_COUNT];

    /** Number of valid entries in @p segments. Must equal QX_SEGMENT_COUNT. */
    uint32_t            segment_count;
} QXManifestLimit;

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXManifestEthics
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Parsed ethics block from the manifest.
 *
 * All five flags must be QX_TRUE (validated by MFT-0011).
 * Corresponds to the JSON key "ethics".
 */
typedef struct QXManifestEthics {
    QXBool  dark_patterns_prohibited;   /**< Dark patterns prohibited.    */
    QXBool  deceptive_flows_prohibited; /**< Deceptive flows prohibited.  */
    QXBool  manipulative_ux_prohibited; /**< Manipulative UX prohibited.  */
    QXBool  privacy_first_design;       /**< Privacy-first design active. */
    QXBool  transparent_data_usage;     /**< Transparent data usage.      */

    /**
     * @brief QX_TRUE if and only if all five ethics flags are QX_TRUE.
     *
     * Computed by qx_manifest_parse(); not read from JSON.
     */
    QXBool  is_fully_ethical;
} QXManifestEthics;

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXManifestNativeCapability
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief A single native capability declaration within the manifest.
 *
 * Corresponds to one entry in "creativity.nativeCapabilities".
 */
typedef struct QXManifestNativeCapability {
    /** Capability identifier used by QXNativeRegistry (null-terminated). */
    char    id[QX_MANIFEST_CAPABILITY_ID_MAX];

    /** Human-readable capability name. */
    char    name[QX_MANIFEST_CAPABILITY_ID_MAX];

    /**
     * @brief Whether the application declares this capability as intentional.
     *
     * A declared capability that is not active at runtime degrades
     * QX_SIGNAL_NATIVE_COVERAGE and may trigger a Law 7 violation.
     */
    QXBool  declared;
} QXManifestNativeCapability;

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXManifestCreativity
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Parsed creativity block from the manifest.
 *
 * Corresponds to the JSON key "creativity".
 */
typedef struct QXManifestCreativity {
    /**
     * @brief Native-first policy flag.
     *
     * Must be QX_TRUE (validated by MFT-0012). When QX_FALSE the manifest
     * is rejected; Law 7 immediately records a FATAL violation.
     */
    QXBool  native_first_policy;

    /**
     * @brief Target ratio of active to declared native capabilities.
     *
     * Range [0.0, 1.0]. Engine raises a Law 7 WARNING when measured
     * native_coverage_ratio drops below this value.
     */
    double  native_utilisation_target;

    /** Declared native capabilities (up to QX_MANIFEST_CAPABILITY_CAP). */
    QXManifestNativeCapability  native_capabilities[QX_MANIFEST_CAPABILITY_CAP];

    /** Number of valid entries in @p native_capabilities. */
    uint32_t                    native_capability_count;
} QXManifestCreativity;

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXManifestKnowledge
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Parsed knowledge block from the manifest.
 *
 * Corresponds to the JSON key "knowledge".
 */
typedef struct QXManifestKnowledge {
    /**
     * @brief Interval between cognitive snapshots in seconds.
     *
     * Must be > 0. Recommended: QX_SNAPSHOT_INTERVAL_SEC (5 s).
     */
    double  snapshot_interval_seconds;

    /**
     * @brief Minimum required knowledge score (0.0 – 100.0).
     *
     * Recommended minimum: QX_MIN_KNOWLEDGE_SCORE (90.0).
     */
    double  min_knowledge_score;

    /**
     * @brief Whether telemetry reporting is enabled.
     *
     * When QX_TRUE the engine emits structured telemetry events for each
     * evaluation cycle, pressure event, and snapshot.
     */
    QXBool  telemetry_enabled;
} QXManifestKnowledge;

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXManifestEconomy
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Parsed economy block from the manifest.
 *
 * Corresponds to the JSON key "economy".
 */
typedef struct QXManifestEconomy {
    /**
     * @brief Maximum network data budget in megabytes per session.
     *
     * Must be > 0 (validated by MFT-0013).
     */
    double  network_budget_mb_per_session;

    /**
     * @brief Maximum permitted battery drain as a percentage per 10 minutes.
     *
     * Must be ≤ QX_MAX_BATTERY_DRAIN_PCT (10.0) (validated by MFT-0014).
     */
    double  battery_drain_max_pct_per_10min;

    /**
     * @brief Commerce model identifier string.
     *
     * Informational only. Examples: "freemium", "subscription", "one-time".
     */
    char    commerce_model[QX_MANIFEST_COMMERCE_MAX];

    /**
     * @brief Maximum permitted network redundancy as a percentage of traffic.
     *
     * Recommended maximum: QX_MAX_NETWORK_REDUNDANCY (10.0).
     */
    double  network_redundancy_max_pct;
} QXManifestEconomy;

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXManifestVerticals
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Parsed verticals block from the manifest.
 *
 * Declares the application domain for analytics and constitutional tuning.
 * Corresponds to the JSON key "verticals".
 */
typedef struct QXManifestVerticals {
    /**
     * @brief Primary vertical classification.
     *
     * Examples: "fintech", "healthtech", "edtech", "gaming", "productivity".
     */
    char    primary[QX_MANIFEST_VERTICAL_MAX];

    /** Secondary vertical classifications (up to QX_MANIFEST_VERTICAL_SEC_MAX). */
    char    secondary[QX_MANIFEST_VERTICAL_SEC_MAX][QX_MANIFEST_VERTICAL_MAX];

    /** Number of valid entries in @p secondary. */
    uint32_t secondary_count;
} QXManifestVerticals;

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXManifestMeta
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Parsed meta block from the manifest.
 *
 * Contains document provenance information.
 * Corresponds to the JSON key "meta".
 */
typedef struct QXManifestMeta {
    /** QXEngine spec version this manifest targets. Expected: "1.0.0". */
    char    spec_version[QX_MANIFEST_SPEC_VER_MAX];

    /** Tool or system that generated this manifest document. */
    char    generated_by[QX_MANIFEST_GENERATED_BY_MAX];

    /**
     * @brief ISO-8601 timestamp when this manifest was last validated.
     *
     * Empty string when absent.
     */
    char    validated_at[QX_MANIFEST_DATE_MAX];
} QXManifestMeta;

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – QXManifestValidationError
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief A single manifest validation error entry.
 */
typedef struct QXManifestValidationError {
    /**
     * @brief Structured error code string.
     *
     * Format: "MFT-XXXX". Null-terminated. Max 16 bytes.
     */
    char    code[16];

    /** Human-readable description of the validation failure. Max 256 bytes. */
    char    message[256];

    /**
     * @brief JSON field path that caused the error.
     *
     * Example: "limit.segments[2].hardLimitMB". Max 128 bytes.
     */
    char    field_path[128];
} QXManifestValidationError;

/**
 * @brief Collection of validation errors from qx_manifest_validate().
 */
typedef struct QXManifestValidationResult {
    /** QX_TRUE if the manifest passed all validation rules. */
    QXBool                      is_valid;

    /** Number of valid entries in @p errors. */
    uint32_t                    error_count;

    /** Validation errors (up to QX_MANIFEST_MAX_ERRORS). */
    QXManifestValidationError   errors[QX_MANIFEST_MAX_ERRORS];
} QXManifestValidationResult;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* QXENGINE_MANIFEST_QX_MANIFEST_TYPES_H */
