/* =============================================================================
 * platforms/ios/bridge/qx_swift_bridge.cpp
 * QXEngine Core – iOS Swift Bridge
 * Repository : https://github.com/qxengine/qxengine-core
 * Owner      : Masa Bayu
 * Created    : 2026-05-24
 * Purpose    : Thin C wrapper layer consumed by Swift via a bridging header.
 *              All functions follow the qx_ios_* naming convention and map
 *              1-to-1 to qx_engine_* C ABI calls. No ObjC runtime required.
 *              Swift callers store the engine handle as an OpaquePointer.
 * =========================================================================== */
#include <cstring>
#include <cstdint>
#include <cstdio>

#include "qxengine/qxengine.h"
#include "qxengine/manifest/qx_manifest.h"
#include "qxengine/law/qx_law_report.h"
#include "qxengine/intelligence/qx_snapshot_types.h"

/* -------------------------------------------------------------------------- */
/* Macros                                                                      */
/* -------------------------------------------------------------------------- */
#define QX_IOS_LOG(fmt, ...)  std::fprintf(stderr, "[QXEngine/iOS] " fmt "\n", ##__VA_ARGS__)
#define QX_IOS_ERR(fmt, ...)  std::fprintf(stderr, "[QXEngine/iOS][ERR] " fmt "\n", ##__VA_ARGS__)

/* -------------------------------------------------------------------------- */
/* Helper – null guard                                                         */
/* -------------------------------------------------------------------------- */
static inline bool qx_ios_null_guard(const void* ptr, const char* name)
{
    if (!ptr) {
        QX_IOS_ERR("null pointer: %s", name);
        return true;
    }
    return false;
}

/* ========================================================================== */
/* Engine lifecycle                                                            */
/* ========================================================================== */

/* qx_ios_engine_create
 * Parameters : manifest_json – UTF-8 JSON string (caller owns)
 * Returns    : opaque engine handle, or NULL on failure
 * Swift sig  : qx_ios_engine_create(manifest_json: UnsafePointer<CChar>?)
 *              -> OpaquePointer?
 */
extern "C" void* qx_ios_engine_create(const char* manifest_json)
{
    if (qx_ios_null_guard(manifest_json, "manifest_json")) return nullptr;

    QXManifestHandle manifest = nullptr;
    QXManifestValidationResult result;
    std::memset(&result, 0, sizeof(result));

    QXError err = qx_manifest_parse(manifest_json, &manifest, &result);
    if (err != QX_OK || !result.is_valid) {
        QX_IOS_ERR("manifest parse failed error_count=%u", result.error_count);
        return nullptr;
    }

    QXEngineConfig cfg;
    std::memset(&cfg, 0, sizeof(cfg));
    cfg.manifest = manifest;

    QXEngineHandle engine = nullptr;
    err = qx_engine_create(&cfg, &engine);
    if (err != QX_OK || !engine) {
        QX_IOS_ERR("qx_engine_create failed err=%d", err);
        qx_manifest_destroy(manifest);
        return nullptr;
    }

    QX_IOS_LOG("engine created handle=%p", static_cast<void*>(engine));
    return static_cast<void*>(engine);
}

/* qx_ios_engine_destroy
 * Swift sig  : qx_ios_engine_destroy(handle: OpaquePointer?)
 */
extern "C" void qx_ios_engine_destroy(void* handle)
{
    if (qx_ios_null_guard(handle, "handle")) return;
    qx_engine_destroy(static_cast<QXEngineHandle>(handle));
    QX_IOS_LOG("engine destroyed handle=%p", handle);
}

/* ========================================================================== */
/* Allocation                                                                  */
/* ========================================================================== */

/* qx_ios_engine_alloc
 * Returns    : opaque leaf handle, or NULL on failure
 * Swift sig  : qx_ios_engine_alloc(handle:segmentId:sizeBytes:label:evictClass:)
 *              -> OpaquePointer?
 */
extern "C" void* qx_ios_engine_alloc(void*       handle,
                                      uint8_t     segment_id,
                                      size_t      size_bytes,
                                      const char* label,
                                      int         evict_class)
{
    if (qx_ios_null_guard(handle, "handle")) return nullptr;

    QXLeafHandle leaf = nullptr;
    QXError err = qx_engine_alloc(
        static_cast<QXEngineHandle>(handle),
        segment_id,
        size_bytes,
        label ? label : "",
        static_cast<QXEvictClass>(evict_class),
        &leaf
    );
    if (err != QX_OK || !leaf) {
        QX_IOS_ERR("qx_engine_alloc failed err=%d", err);
        return nullptr;
    }
    return static_cast<void*>(leaf);
}

/* qx_ios_engine_free
 * Returns    : 1 on success, 0 on failure
 * Swift sig  : qx_ios_engine_free(handle:leafHandle:) -> Int32
 */
extern "C" int qx_ios_engine_free(void* handle, void* leaf_handle)
{
    if (qx_ios_null_guard(handle,      "handle"))      return 0;
    if (qx_ios_null_guard(leaf_handle, "leaf_handle")) return 0;
    return qx_engine_free(
        static_cast<QXEngineHandle>(handle),
        static_cast<QXLeafHandle>(leaf_handle)
    ) == QX_OK ? 1 : 0;
}

/* ========================================================================== */
/* Evaluation                                                                  */
/* ========================================================================== */

/* qx_ios_engine_evaluate
 * Returns    : health score [0.0 – 100.0], or -1.0 on failure
 * Swift sig  : qx_ios_engine_evaluate(handle: OpaquePointer?) -> Double
 */
extern "C" double qx_ios_engine_evaluate(void* handle)
{
    if (qx_ios_null_guard(handle, "handle")) return -1.0;

    QXLawReport report;
    std::memset(&report, 0, sizeof(report));
    if (qx_engine_evaluate(static_cast<QXEngineHandle>(handle),
                           &report) != QX_OK) return -1.0;

    QX_IOS_LOG("evaluate health_score=%.2f", report.health_score);
    return report.health_score;
}

/* ========================================================================== */
/* Snapshot                                                                    */
/* ========================================================================== */

/* qx_ios_engine_snapshot
 * Returns    : knowledge score [0.0 – 100.0], or -1.0 on failure
 * Swift sig  : qx_ios_engine_snapshot(handle: OpaquePointer?) -> Double
 */
extern "C" double qx_ios_engine_snapshot(void* handle)
{
    if (qx_ios_null_guard(handle, "handle")) return -1.0;

    QXCognitiveSnapshot snap;
    std::memset(&snap, 0, sizeof(snap));
    if (qx_engine_snapshot(static_cast<QXEngineHandle>(handle),
                           &snap) != QX_OK) return -1.0;

    return snap.knowledge_score;
}

/* ========================================================================== */
/* Pressure                                                                    */
/* ========================================================================== */

/* qx_ios_engine_pressure_warning
 * Parameters : ram_used_ratio – fraction of RAM in use [0.0 – 1.0]
 * Returns    : 1 on success, 0 on failure
 * Swift sig  : qx_ios_engine_pressure_warning(handle:ramUsedRatio:) -> Int32
 */
extern "C" int qx_ios_engine_pressure_warning(void* handle, double ram_used_ratio)
{
    if (qx_ios_null_guard(handle, "handle")) return 0;
    QX_IOS_LOG("memory pressure warning ram_used=%.2f", ram_used_ratio);
    return qx_engine_pressure_ios(
        static_cast<QXEngineHandle>(handle), ram_used_ratio
    ) == QX_OK ? 1 : 0;
}

/* ========================================================================== */
/* Stats                                                                       */
/* ========================================================================== */

/* qx_ios_engine_stats
 * Fills caller-supplied QXEngineStats struct.
 * Returns    : 1 on success, 0 on failure
 * Swift sig  : qx_ios_engine_stats(handle:stats:) -> Int32
 */
extern "C" int qx_ios_engine_stats(void* handle, QXEngineStats* out_stats)
{
    if (qx_ios_null_guard(handle,    "handle"))    return 0;
    if (qx_ios_null_guard(out_stats, "out_stats")) return 0;
    return qx_engine_stats(
        static_cast<QXEngineHandle>(handle), out_stats
    ) == QX_OK ? 1 : 0;
}
