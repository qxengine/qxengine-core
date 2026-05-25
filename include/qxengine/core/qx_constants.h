/* ============================================================================
 * QXEngine Core – qx_constants.h
 * Owner:      Masa Bayu
 * Created:    2026-05-24
 * Description: Constitutional constants for the QXEngine C++ core engine.
 *              This is the single source of truth for all numeric thresholds,
 *              architectural constants, and scoring parameters.
 *              All values are identical across iOS, Android, and CLI.
 *              No platform may override or redefine these values.
 *
 *              Constitutional authority:
 *                These constants are derived directly from the eight
 *                constitutional laws defined in qxengine-spec.
 *                Any change to these values requires a spec amendment
 *                and a major version bump.
 *
 * Repository: https://github.com/qxengine/qxengine-core
 * ============================================================================
 */

#ifndef QX_CONSTANTS_H
#define QX_CONSTANTS_H

#include <qxengine/core/qx_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * MARK: – Architecture constants
 * Defines the structural dimensions of the QXEngine memory grid.
 * ============================================================================ */

/**
 * @brief Number of constitutional memory segments.
 *
 * The nine canonical segments are:
 *   QLM-CORE, QLM-UI, QLM-DATA, QLM-IMG, QLM-NET,
 *   QLM-AI, QLM-SEC, QLM-LOG, QLM-TEMP
 */
#define QX_SEGMENT_COUNT            9u

/* Segment ID constants — must match QX_SEGMENT_COUNT (9) */
#define QX_SEGMENT_ID_CORE   0u
#define QX_SEGMENT_ID_UI     1u
#define QX_SEGMENT_ID_DATA   2u
#define QX_SEGMENT_ID_IMG    3u
#define QX_SEGMENT_ID_NET    4u
#define QX_SEGMENT_ID_AI     5u
#define QX_SEGMENT_ID_SEC    6u
#define QX_SEGMENT_ID_LOG    7u
#define QX_SEGMENT_ID_TEMP   8u

/**
 * @brief Total number of QLM slots across all segments.
 *
 * Slot distribution:
 *   QLM-CORE, QLM-UI, QLM-DATA, QLM-IMG, QLM-NET, QLM-AI → 7 slots each
 *   QLM-SEC, QLM-LOG, QLM-TEMP                            → 6 slots each
 *   Total: (6 × 7) + (3 × 6) = 42 + 18 = 60
 */
#define QX_SLOT_COUNT               60u

/**
 * @brief Maximum number of leaf allocations tracked in the knowledge log.
 *
 * Derived from the constitutional leaf taxonomy:
 *   9 segments × average 15.9 leaves per segment ≈ 143
 */
#define QX_LEAF_COUNT               143u

/**
 * @brief Slots allocated to high-priority segments.
 * Applies to: QLM-CORE, QLM-UI, QLM-DATA, QLM-IMG, QLM-NET, QLM-AI
 */
#define QX_SLOTS_HIGH_PRIORITY      7u

/**
 * @brief Slots allocated to low-priority segments.
 * Applies to: QLM-SEC, QLM-LOG, QLM-TEMP
 */
#define QX_SLOTS_LOW_PRIORITY       6u

/* ============================================================================
 * MARK: – Segment IDs
 * Canonical null-terminated string identifiers for the nine segments.
 * These must match the manifest JSON segment id fields exactly.
 * ============================================================================ */

#define QX_SEGMENT_CORE             "QLM-CORE"
#define QX_SEGMENT_UI               "QLM-UI"
#define QX_SEGMENT_DATA             "QLM-DATA"
#define QX_SEGMENT_IMG              "QLM-IMG"
#define QX_SEGMENT_NET              "QLM-NET"
#define QX_SEGMENT_AI               "QLM-AI"
#define QX_SEGMENT_SEC              "QLM-SEC"
#define QX_SEGMENT_LOG              "QLM-LOG"
#define QX_SEGMENT_TEMP             "QLM-TEMP"

/* ============================================================================
 * MARK: – Intelligence cycle constants (Law 5 – Knowledge)
 * ============================================================================ */

/**
 * @brief Cognitive snapshot interval in seconds.
 *
 * The QIADirector evaluates all eight constitutional laws every
 * QX_SNAPSHOT_INTERVAL_SECONDS seconds and records the result
 * as a QXCognitiveSnapshot in the telemetry log.
 */
#define QX_SNAPSHOT_INTERVAL_SEC    5u

/**
 * @brief Maximum number of snapshots retained in the ring buffer.
 *
 * 720 snapshots × 5 seconds = 3600 seconds = 1 hour of history.
 */
#define QX_SNAPSHOT_HISTORY_CAP     720u

/**
 * @brief Maximum number of telemetry events in the ring buffer.
 */
#define QX_TELEMETRY_EVENT_CAP      1000u

/**
 * @brief Maximum number of violation events in the ring buffer.
 */
#define QX_VIOLATION_LOG_CAP        1000u

/* ============================================================================
 * MARK: – Law 1 – Pattern constants
 * ============================================================================ */

/**
 * @brief Minimum allocation label length in bytes (UTF-8).
 * Labels shorter than this are rejected with QX_ERR_LABEL_TOO_SHORT.
 */
#define QX_LABEL_MIN_LENGTH         3u

/**
 * @brief Maximum allocation label length in bytes (UTF-8).
 * Labels longer than this are rejected with QX_ERR_LABEL_TOO_LONG.
 */
#define QX_LABEL_MAX_LENGTH         128u

/**
 * @brief Required pattern score for certification.
 *
 * Any unlabelled allocation reduces the pattern score to 0.0.
 * 100% of allocations must be labelled to achieve a non-zero score.
 */
#define QX_REQUIRED_PATTERN_SCORE   (100.0)

/* ============================================================================
 * MARK: – Law 2 – Limit constants
 * ============================================================================ */

/**
 * @brief Minimum memory soft limit in megabytes.
 * Manifests declaring a soft limit below this value are rejected.
 */
#define QX_MIN_SOFT_LIMIT_MB        64u

/**
 * @brief Minimum memory hard limit in megabytes.
 * Manifests declaring a hard limit below this value are rejected.
 */
#define QX_MIN_HARD_LIMIT_MB        128u

/**
 * @brief Pressure Tier 2 threshold – soft budget utilisation percentage.
 * At or above this value, Class D eviction is triggered.
 */
#define QX_TIER_2_THRESHOLD_PCT     (70.0)

/**
 * @brief Pressure Tier 3 threshold – soft budget utilisation percentage.
 * At or above this value, Class D + C eviction is triggered.
 */
#define QX_TIER_3_THRESHOLD_PCT     (85.0)

/**
 * @brief Pressure Tier 4 threshold – soft budget utilisation percentage.
 * At or above this value, Class D + C + B eviction is triggered.
 */
#define QX_TIER_4_THRESHOLD_PCT     (95.0)

/* ============================================================================
 * MARK: – Law 3 – Pairs constants
 * ============================================================================ */

/**
 * @brief Minimum acceptable pairs ratio (deallocations / allocations).
 * Ratios below this threshold trigger a warning violation.
 */
#define QX_MIN_PAIRS_RATIO          (0.5)

/**
 * @brief Orphaned byte warning threshold.
 * When orphaned bytes exceed this fraction of used bytes, a warning
 * violation is emitted.
 */
#define QX_ORPHAN_WARNING_PCT       (0.10)

/**
 * @brief Orphaned byte critical threshold.
 * When orphaned bytes exceed this fraction of used bytes, a critical
 * violation is emitted.
 */
#define QX_ORPHAN_CRITICAL_PCT      (0.25)

/**
 * @brief Idle time in seconds after which a leaf is considered orphaned.
 * Leaves idle longer than this are counted as orphaned bytes (Law 8).
 */
#define QX_ORPHAN_IDLE_SEC          60ULL

/* ============================================================================
 * MARK: – Law 4 – Equilibrium constants
 * ============================================================================ */

/**
 * @brief Segment overload threshold percentage.
 * A segment utilising more than this percentage of its soft budget
 * relative to other segments triggers an equilibrium violation.
 */
#define QX_EQUILIBRIUM_OVERLOAD_PCT  (80.0)

/**
 * @brief Segment starvation threshold percentage.
 * A segment utilising less than this percentage of its soft budget
 * when other segments are overloaded triggers a warning violation.
 */
#define QX_EQUILIBRIUM_STARVE_PCT    (20.0)

/* ============================================================================
 * MARK: – Law 5 – Knowledge constants
 * ============================================================================ */

/**
 * @brief Minimum knowledge score for certification.
 * The knowledge score is: logged mandatory events / required events × 100.
 * Scores below this threshold trigger a knowledge violation.
 */
#define QX_MIN_KNOWLEDGE_SCORE      (90.0)

/* ============================================================================
 * MARK: – Law 6 – Ethics constants
 * ============================================================================ */

/**
 * @brief Required ethics score for certification.
 * All five ethics flags must be true. Any false flag yields 0.0.
 */
#define QX_REQUIRED_ETHICS_SCORE    (100.0)

/* ============================================================================
 * MARK: – Law 7 – Creativity constants
 * ============================================================================ */

/**
 * @brief Minimum native utilisation target (fraction, not percentage).
 * At least 50% of declared native capabilities must be active
 * to achieve a full creativity score.
 */
#define QX_MIN_NATIVE_UTILISATION   (0.5)

/* ============================================================================
 * MARK: – Law 8 – Economy constants
 * ============================================================================ */

/**
 * @brief Maximum battery drain percentage per 10-minute window.
 * Drain above this value triggers a critical economy violation.
 */
#define QX_MAX_BATTERY_DRAIN_PCT    (10.0)

/**
 * @brief Maximum network redundancy percentage.
 * Redundancy above this value triggers an economy violation.
 */
#define QX_MAX_NETWORK_REDUNDANCY   (10.0)

/**
 * @brief Economy warning threshold percentage.
 * Economy metrics above this fraction of their maximum trigger warnings.
 */
#define QX_ECONOMY_WARNING_PCT      (75.0)

/**
 * @brief Economy critical threshold percentage.
 * Economy metrics above this fraction of their maximum trigger critical
 * violations.
 */
#define QX_ECONOMY_CRITICAL_PCT     (100.0)

/* ============================================================================
 * MARK: – Health score weights
 * Weights must sum to exactly 1.0.
 * Any change requires a spec amendment and major version bump.
 * ============================================================================ */

#define QX_WEIGHT_PATTERN           (0.15)  /**< Law 1 – 15% */
#define QX_WEIGHT_LIMIT             (0.20)  /**< Law 2 – 20% */
#define QX_WEIGHT_PAIRS             (0.15)  /**< Law 3 – 15% */
#define QX_WEIGHT_EQUILIBRIUM       (0.10)  /**< Law 4 – 10% */
#define QX_WEIGHT_KNOWLEDGE         (0.15)  /**< Law 5 – 15% */
#define QX_WEIGHT_ETHICS            (0.10)  /**< Law 6 – 10% */
#define QX_WEIGHT_CREATIVITY        (0.10)  /**< Law 7 – 10% */
#define QX_WEIGHT_ECONOMY           (0.05)  /**< Law 8 –  5% */

/* ============================================================================
 * MARK: – Certification tier thresholds
 * ============================================================================ */

#define QX_CERT_SOVEREIGN_MIN       (95.0)  /**< SOVEREIGN     ≥ 95% */
#define QX_CERT_PROFESSIONAL_MIN    (80.0)  /**< PROFESSIONAL  ≥ 80% */
#define QX_CERT_STANDARD_MIN        (60.0)  /**< STANDARD      ≥ 60% */

/* ============================================================================
 * MARK: – Device scale factors
 * Applied to manifest memory limits based on device RAM class.
 * ============================================================================ */

#define QX_SCALE_HIGH_END           (1.00)  /**< ≥ 6 GB RAM */
#define QX_SCALE_MID_RANGE          (0.75)  /**< 3–6 GB RAM */
#define QX_SCALE_ENTRY_LEVEL        (0.60)  /**< < 3 GB RAM */

#define QX_RAM_HIGH_END_BYTES       6442450944ULL  /**< 6 GB */
#define QX_RAM_MID_RANGE_BYTES      3221225472ULL  /**< 3 GB */

/* ============================================================================
 * MARK: – Schema and identity constants
 * ============================================================================ */

#define QX_SCHEMA_URL_CONST         "https://qxengine.io/manifest/v1.0/schema.json"
#define QX_OWNER_CONST              "Masa Bayu"
#define QX_ENGINE_VERSION_CONST     "1.0.0"
#define QX_SPEC_VERSION_CONST       "1.0.0"

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* QX_CONSTANTS_H */
