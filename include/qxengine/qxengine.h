/* ============================================================================
 * QXEngine Core – qxengine.h
 * Owner:      Masa Bayu
 * Created:    2026-05-24
 * Description: Master include file for the QXEngine C++ core engine.
 *              This is the ONLY header consumers need to include.
 *              All public API is reachable through this single entry point.
 *
 *              Usage:
 *                #include <qxengine/qxengine.h>
 *
 *              Platform integration:
 *                iOS  – include via XCFramework umbrella header
 *                Android – include via NDK CMake integration
 *                CLI  – include directly
 *
 * Repository: https://github.com/qxengine/qxengine-core
 * ============================================================================
 */

#ifndef QXENGINE_H
#define QXENGINE_H

/* --------------------------------------------------------------------------
 * C ABI boundary declaration
 * All QXEngine public API is pure C for maximum platform compatibility.
 * C++ internal implementation is hidden behind opaque handles.
 * -------------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 * Core – types, constants, errors, version
 * -------------------------------------------------------------------------- */
#include <qxengine/core/qx_types.h>
#include <qxengine/core/qx_version.h>
#include <qxengine/core/qx_error.h>
#include <qxengine/core/qx_constants.h>

/* --------------------------------------------------------------------------
 * Memory – leaf, segment, memloc, pressure
 * -------------------------------------------------------------------------- */
#include <qxengine/memory/qx_leaf.h>
#include <qxengine/memory/qx_segment.h>
#include <qxengine/memory/qx_memloc.h>
#include <qxengine/memory/qx_pressure.h>

/* --------------------------------------------------------------------------
 * Law – types and enforcer
 * -------------------------------------------------------------------------- */
#include <qxengine/law/qx_law_types.h>
#include <qxengine/law/qx_law_report.h>
#include <qxengine/law/qx_law_enforcer.h>

/* --------------------------------------------------------------------------
 * Intelligence – cognitive snapshot
 * -------------------------------------------------------------------------- */
#include <qxengine/intelligence/qx_snapshot.h>

/* --------------------------------------------------------------------------
 * Manifest – manifest model and validation
 * -------------------------------------------------------------------------- */
#include <qxengine/manifest/qx_manifest.h>

/* --------------------------------------------------------------------------
 * Telemetry – knowledge log and audit
 * -------------------------------------------------------------------------- */
#include <qxengine/telemetry/qx_telemetry.h>

/* --------------------------------------------------------------------------
 * Engine lifecycle – top-level engine init and shutdown
 * -------------------------------------------------------------------------- */

/**
 * @brief Opaque handle to a QXEngine instance.
 *
 * One engine instance manages one application manifest.
 * Create with qx_engine_create(). Destroy with qx_engine_destroy().
 */
typedef struct QXEngine_s* QXEngineHandle;

/**
 * @brief Engine lifecycle state.
 */
typedef enum QXEngineState {
    QX_ENGINE_STATE_UNCONFIGURED = 0,
    QX_ENGINE_STATE_CONFIGURED   = 1,
    QX_ENGINE_STATE_STARTING     = 2,
    QX_ENGINE_STATE_RUNNING      = 3,
    QX_ENGINE_STATE_PAUSED       = 4,
    QX_ENGINE_STATE_STOPPING     = 5,
    QX_ENGINE_STATE_STOPPED      = 6
} QXEngineState;

/**
 * @brief Engine configuration passed to qx_engine_create().
 *
 * @note manifest_json must remain valid for the lifetime of the engine.
 *       The engine does not take ownership of the pointer.
 */
typedef struct QXEngineConfig {
    const char*       manifest_json;        /**< UTF-8 JSON manifest string   */
    uint32_t          manifest_json_length; /**< byte length of manifest_json  */
    uint64_t          device_ram_bytes;     /**< physical RAM (from platform)  */
    QXBool            verify_integrity;     /**< verify manifest SHA-256       */
    QXBool            verbose;              /**< enable verbose logging        */
} QXEngineConfig;

/**
 * @brief Creates and configures a new QXEngine instance.
 *
 * Parses the manifest JSON, validates all eight constitutional laws,
 * initialises memory segments and the cognitive evaluation loop.
 *
 * @param[in]  config     Engine configuration. Must not be NULL.
 * @param[out] out_engine Written with the new engine handle on success.
 *
 * @return QX_OK on success.
 * @return QX_ERR_MANIFEST_PARSE   if manifest JSON is malformed.
 * @return QX_ERR_MANIFEST_INVALID if manifest violates constitutional rules.
 * @return QX_ERR_NULL_HANDLE      if config or out_engine is NULL.
 * @return QX_ERR_INTERNAL         on unexpected internal failure.
 *
 * @note Not thread-safe. Call from one thread at startup.
 */
QXResult qx_engine_create(
    const QXEngineConfig* config,
    QXEngineHandle*       out_engine
);

/**
 * @brief Starts the constitutional evaluation loop.
 *
 * Begins the 5-second cognitive snapshot cycle (Law 5 – Knowledge).
 * Engine must be in QX_ENGINE_STATE_CONFIGURED state.
 *
 * @param[in] engine Valid engine handle.
 *
 * @return QX_OK on success.
 * @return QX_ERR_ALREADY_RUNNING if engine is already running.
 * @return QX_ERR_NULL_HANDLE     if engine is NULL.
 *
 * @note Not thread-safe. Call from one thread.
 */
QXResult qx_engine_start(QXEngineHandle engine);

/**
 * @brief Pauses the constitutional evaluation loop.
 *
 * Suspends the 5-second snapshot cycle. Memory operations remain active.
 *
 * @param[in] engine Valid engine handle.
 *
 * @return QX_OK on success.
 * @return QX_ERR_NOT_READY   if engine is not running.
 * @return QX_ERR_NULL_HANDLE if engine is NULL.
 *
 * @note Thread-safe.
 */
QXResult qx_engine_pause(QXEngineHandle engine);

/**
 * @brief Resumes the constitutional evaluation loop.
 *
 * @param[in] engine Valid engine handle.
 *
 * @return QX_OK on success.
 * @return QX_ERR_NOT_READY   if engine is not paused.
 * @return QX_ERR_NULL_HANDLE if engine is NULL.
 *
 * @note Thread-safe.
 */
QXResult qx_engine_resume(QXEngineHandle engine);

/**
 * @brief Stops the engine and releases all internal resources.
 *
 * After this call the engine handle is invalid.
 * The caller must still call qx_engine_destroy() to free the handle itself.
 *
 * @param[in] engine Valid engine handle.
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if engine is NULL.
 *
 * @note Not thread-safe. Call from one thread at shutdown.
 */
QXResult qx_engine_stop(QXEngineHandle engine);

/**
 * @brief Destroys a QXEngine handle and frees all associated memory.
 *
 * Safe to call on a stopped or unconfigured engine.
 * Passing NULL is a no-op.
 *
 * @param[in] engine Engine handle to destroy. May be NULL.
 *
 * @note Not thread-safe. Call from one thread at shutdown.
 */
void qx_engine_destroy(QXEngineHandle engine);

/**
 * @brief Returns the current lifecycle state of the engine.
 *
 * @param[in]  engine    Valid engine handle.
 * @param[out] out_state Written with the current state on success.
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if engine or out_state is NULL.
 *
 * @note Thread-safe.
 */
QXResult qx_engine_state(
    QXEngineHandle  engine,
    QXEngineState*  out_state
);

/**
 * @brief Forces an immediate constitutional evaluation outside the
 *        5-second cycle. Useful for testing and diagnostic tools.
 *
 * @param[in] engine Valid engine handle.
 *
 * @return QX_OK on success.
 * @return QX_ERR_NOT_READY   if engine is not running.
 * @return QX_ERR_NULL_HANDLE if engine is NULL.
 *
 * @note Thread-safe.
 */
QXResult qx_engine_evaluate_now(QXEngineHandle engine);

/* --------------------------------------------------------------------------
 * Memory API – allocation, deallocation, touch
 * These are thin forwarders to qx_memloc_* functions.
 * Kept here for ergonomic access from platform bridges.
 * -------------------------------------------------------------------------- */

/**
 * @brief Allocates a leaf in the specified segment.
 *
 * Enforces Laws 1–4 on every call.
 *
 * @param[in]  engine     Valid running engine handle.
 * @param[in]  label      Non-null UTF-8 label, length [3, 128].
 * @param[in]  segment_id One of the nine canonical QLM-* segment IDs.
 * @param[in]  leaf_class QXLeafClassId (0=A, 1=B, 2=C, 3=D).
 * @param[in]  size_bytes Allocation size in bytes. Must be > 0.
 * @param[out] out_leaf   Written with the new leaf handle on success.
 *
 * @return QX_OK on success.
 * @return QX_ERR_LABEL_BLANK      if label is empty or whitespace.
 * @return QX_ERR_LABEL_TOO_SHORT  if label length < 3.
 * @return QX_ERR_LABEL_TOO_LONG   if label length > 128.
 * @return QX_ERR_UNKNOWN_SEGMENT  if segment_id is not recognised.
 * @return QX_ERR_NO_SLOT          if segment has no available slots.
 * @return QX_ERR_HARD_BUDGET      if allocation exceeds hard limit.
 * @return QX_ERR_NOT_READY        if engine is not running.
 * @return QX_ERR_NULL_HANDLE      if engine or out_leaf is NULL.
 *
 * @note Thread-safe.
 */
QXResult qx_engine_allocate(
    QXEngineHandle   engine,
    const char*      label,
    const char*      segment_id,
    QXLeafClassId    leaf_class,
    QXSize           size_bytes,
    QXLeafHandle*    out_leaf
);

/**
 * @brief Deallocates a leaf by ID.
 *
 * Enforces Law 3 (Pairs) – double-free returns QX_ERR_DOUBLE_FREE.
 *
 * @param[in] engine  Valid running engine handle.
 * @param[in] leaf_id Null-terminated UUID string of the leaf to free.
 *
 * @return QX_OK on success.
 * @return QX_ERR_LEAF_NOT_FOUND if leaf_id does not exist.
 * @return QX_ERR_DOUBLE_FREE    if leaf was already deallocated.
 * @return QX_ERR_NOT_READY      if engine is not running.
 * @return QX_ERR_NULL_HANDLE    if engine or leaf_id is NULL.
 *
 * @note Thread-safe.
 */
QXResult qx_engine_deallocate(
    QXEngineHandle engine,
    const char*    leaf_id
);

/**
 * @brief Updates the LRU timestamp of a leaf (Law 3 – Pairs).
 *
 * @param[in] engine  Valid running engine handle.
 * @param[in] leaf_id Null-terminated UUID string of the leaf to touch.
 *
 * @return QX_OK on success.
 * @return QX_ERR_LEAF_NOT_FOUND if leaf_id does not exist.
 * @return QX_ERR_NULL_HANDLE    if engine or leaf_id is NULL.
 *
 * @note Thread-safe.
 */
QXResult qx_engine_touch(
    QXEngineHandle engine,
    const char*    leaf_id
);

/* --------------------------------------------------------------------------
 * Economy API – Law 8
 * Platform layer feeds OS-measured values into the C++ core.
 * -------------------------------------------------------------------------- */

/**
 * @brief Updates the network redundancy metric (Law 8 – Economy).
 *
 * @param[in] engine          Valid running engine handle.
 * @param[in] redundancy_pct  Network redundancy percentage [0.0, 200.0].
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if engine is NULL.
 *
 * @note Thread-safe.
 */
QXResult qx_engine_update_network_redundancy(
    QXEngineHandle engine,
    double         redundancy_pct
);

/**
 * @brief Updates the battery drain metric (Law 8 – Economy).
 *
 * @param[in] engine       Valid running engine handle.
 * @param[in] drain_pct    Battery drain percentage per 10 min [0.0, 100.0].
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if engine is NULL.
 *
 * @note Thread-safe.
 */
QXResult qx_engine_update_battery_drain(
    QXEngineHandle engine,
    double         drain_pct
);

/* --------------------------------------------------------------------------
 * Memory pressure API
 * Platform layer forwards OS memory pressure signals to the C++ core.
 * -------------------------------------------------------------------------- */

/**
 * @brief Notifies the engine of a system memory pressure event.
 *
 * Maps platform-specific pressure levels to QXEngine pressure tiers
 * and triggers the appropriate eviction cascade.
 *
 * @param[in] engine         Valid running engine handle.
 * @param[in] pressure_level Platform pressure level [1, 4].
 *                           1 = normal, 2 = elevated, 3 = high, 4 = critical.
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if engine is NULL.
 *
 * @note Thread-safe.
 */
QXResult qx_engine_notify_memory_pressure(
    QXEngineHandle engine,
    uint8_t        pressure_level
);

/* --------------------------------------------------------------------------
 * OS memory feed API
 * Platform layer feeds OS-measured memory readings into the C++ core.
 * Called every 5 seconds by the platform adapter.
 * -------------------------------------------------------------------------- */

/**
 * @brief Feeds an OS memory reading into the engine for Law 2 evaluation.
 *
 * @param[in] engine          Valid running engine handle.
 * @param[in] resident_bytes  Process RSS in bytes.
 * @param[in] system_total    System total RAM in bytes.
 * @param[in] system_avail    System available RAM in bytes.
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if engine is NULL.
 *
 * @note Thread-safe.
 */
QXResult qx_engine_feed_memory_reading(
    QXEngineHandle engine,
    uint64_t       resident_bytes,
    uint64_t       system_total,
    uint64_t       system_avail
);

/* --------------------------------------------------------------------------
 * Native capability API – Law 7
 * Platform layer reports which native capabilities are active.
 * -------------------------------------------------------------------------- */

/**
 * @brief Marks a native capability as active (Law 7 – Creativity).
 *
 * @param[in] engine        Valid running engine handle.
 * @param[in] capability_id Null-terminated capability ID string.
 *                          Must match a declared capability in the manifest.
 *
 * @return QX_OK on success.
 * @return QX_ERR_NULL_HANDLE if engine or capability_id is NULL.
 *
 * @note Thread-safe.
 */
QXResult qx_engine_mark_capability_active(
    QXEngineHandle engine,
    const char*    capability_id
);

/* --------------------------------------------------------------------------
 * Integrity flow API
 * Seven-stage integrity state derived from existing runtime evidence.
 * -------------------------------------------------------------------------- */

#define QX_INTEGRITY_STAGE_COUNT 7u

typedef enum QXIntegrityStage {
    QX_INTEGRITY_ACCEPTANCE     = 0,
    QX_INTEGRITY_IMPLEMENTATION = 1,
    QX_INTEGRITY_PATIENCE       = 2,
    QX_INTEGRITY_REFINEMENT     = 3,
    QX_INTEGRITY_ACCURACY       = 4,
    QX_INTEGRITY_TRANQUILITY    = 5,
    QX_INTEGRITY_SINCERITY      = 6
} QXIntegrityStage;

typedef enum QXIntegrityStatus {
    QX_INTEGRITY_STATUS_UNKNOWN = 0,
    QX_INTEGRITY_STATUS_PENDING = 1,
    QX_INTEGRITY_STATUS_NOT_OK  = 2,
    QX_INTEGRITY_STATUS_OK      = 3
} QXIntegrityStatus;

typedef struct QXIntegrityReport {
    QXIntegrityStatus status;
    QXIntegrityStage  current_stage;
    QXBool            stage_passed[QX_INTEGRITY_STAGE_COUNT];
    char              failure_reason[128];
    QXTimestamp       evaluated_at_ms;
    uint64_t          evaluation_count;
    uint64_t          snapshot_count;
    double            health_score;
} QXIntegrityReport;

QXResult qx_engine_integrity_status(
    QXEngineHandle     engine,
    QXIntegrityStatus* out_status
);

QXResult qx_engine_integrity_report(
    QXEngineHandle    engine,
    QXIntegrityReport* out_report
);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* QXENGINE_H */
