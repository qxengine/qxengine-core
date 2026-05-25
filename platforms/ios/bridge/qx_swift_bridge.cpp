/* =============================================================================
 * platforms/ios/bridge/qx_swift_bridge.cpp
 * QXEngine Core – iOS Swift Bridge
 * Repository : https://github.com/qxengine/qxengine-core
 * Owner      : Masa Bayu
 * Created    : 2026-05-24
 * Purpose    : Thin C wrapper consumed by Swift via dynamic symbols or module map.
 * =========================================================================== */

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "qxengine/qxengine.h"

#define QX_IOS_LOG(fmt, ...) std::fprintf(stderr, "[QXEngine/iOS] " fmt "\n", ##__VA_ARGS__)
#define QX_IOS_ERR(fmt, ...) std::fprintf(stderr, "[QXEngine/iOS][ERR] " fmt "\n", ##__VA_ARGS__)

static inline bool qx_ios_null_guard(const void* ptr, const char* name)
{
    if (!ptr) {
        QX_IOS_ERR("null pointer: %s", name);
        return true;
    }
    return false;
}

extern "C" void* qx_ios_engine_create(const char* manifest_json,
                                      uint32_t manifest_json_length,
                                      uint64_t device_ram_bytes,
                                      int verify_integrity,
                                      int verbose)
{
    if (qx_ios_null_guard(manifest_json, "manifest_json")) return nullptr;

    QXEngineConfig cfg{};
    cfg.manifest_json = manifest_json;
    cfg.manifest_json_length = manifest_json_length;
    cfg.device_ram_bytes = device_ram_bytes;
    cfg.verify_integrity = verify_integrity ? QX_TRUE : QX_FALSE;
    cfg.verbose = verbose ? QX_TRUE : QX_FALSE;

    QXEngineHandle engine = nullptr;
    const QXResult rc = qx_engine_create(&cfg, &engine);
    if (rc != QX_OK || !engine) {
        QX_IOS_ERR("qx_engine_create failed rc=%d", rc);
        return nullptr;
    }

    QX_IOS_LOG("engine created handle=%p", static_cast<void*>(engine));
    return static_cast<void*>(engine);
}

extern "C" int qx_ios_engine_start(void* handle)
{
    if (qx_ios_null_guard(handle, "handle")) return QX_ERR_NULL_HANDLE;
    return qx_engine_start(static_cast<QXEngineHandle>(handle));
}

extern "C" int qx_ios_engine_stop(void* handle)
{
    if (qx_ios_null_guard(handle, "handle")) return QX_ERR_NULL_HANDLE;
    return qx_engine_stop(static_cast<QXEngineHandle>(handle));
}

extern "C" void qx_ios_engine_destroy(void* handle)
{
    if (!handle) return;
    qx_engine_destroy(static_cast<QXEngineHandle>(handle));
}

extern "C" int qx_ios_engine_notify_memory_pressure(void* handle,
                                                     uint8_t pressure_level)
{
    if (qx_ios_null_guard(handle, "handle")) return QX_ERR_NULL_HANDLE;
    return qx_engine_notify_memory_pressure(
        static_cast<QXEngineHandle>(handle),
        pressure_level
    );
}

extern "C" int qx_ios_engine_feed_memory_reading(void* handle,
                                                  uint64_t resident_bytes,
                                                  uint64_t system_total,
                                                  uint64_t system_avail)
{
    if (qx_ios_null_guard(handle, "handle")) return QX_ERR_NULL_HANDLE;
    return qx_engine_feed_memory_reading(
        static_cast<QXEngineHandle>(handle),
        resident_bytes,
        system_total,
        system_avail
    );
}

extern "C" int qx_ios_engine_update_battery_drain(void* handle,
                                                   double drain_pct)
{
    if (qx_ios_null_guard(handle, "handle")) return QX_ERR_NULL_HANDLE;
    return qx_engine_update_battery_drain(
        static_cast<QXEngineHandle>(handle),
        drain_pct
    );
}

extern "C" int qx_ios_engine_update_network_redundancy(void* handle,
                                                        double redundancy_pct)
{
    if (qx_ios_null_guard(handle, "handle")) return QX_ERR_NULL_HANDLE;
    return qx_engine_update_network_redundancy(
        static_cast<QXEngineHandle>(handle),
        redundancy_pct
    );
}

extern "C" int qx_ios_engine_mark_capability_active(void* handle,
                                                     const char* capability_id)
{
    if (qx_ios_null_guard(handle, "handle")) return QX_ERR_NULL_HANDLE;
    if (qx_ios_null_guard(capability_id, "capability_id")) return QX_ERR_NULL_HANDLE;
    return qx_engine_mark_capability_active(
        static_cast<QXEngineHandle>(handle),
        capability_id
    );
}

extern "C" int qx_ios_engine_evaluate_now(void* handle)
{
    if (qx_ios_null_guard(handle, "handle")) return QX_ERR_NULL_HANDLE;
    return qx_engine_evaluate_now(static_cast<QXEngineHandle>(handle));
}

extern "C" int qx_ios_engine_integrity_status(void* handle, int* out_status)
{
    if (qx_ios_null_guard(handle, "handle")) return QX_ERR_NULL_HANDLE;
    if (qx_ios_null_guard(out_status, "out_status")) return QX_ERR_NULL_HANDLE;

    QXIntegrityStatus status = QX_INTEGRITY_STATUS_UNKNOWN;
    const QXResult rc = qx_engine_integrity_status(
        static_cast<QXEngineHandle>(handle),
        &status
    );
    if (rc == QX_OK) *out_status = static_cast<int>(status);
    return rc;
}
