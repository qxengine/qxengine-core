/* =============================================================================
 * platforms/android/jni/qx_jni_bridge.cpp
 * QXEngine Core – Android JNI Bridge
 * Repository : https://github.com/qxengine/qxengine-core
 * Owner      : Masa Bayu
 * Created    : 2026-05-24
 * Purpose    : Exposes current QXEngine C ABI to Android Kotlin via JNI.
 * =========================================================================== */

#include <android/log.h>
#include <cstdint>
#include <cstring>
#include <jni.h>

#include "qxengine/qxengine.h"

#define QX_LOG_TAG "QXEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, QX_LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, QX_LOG_TAG, __VA_ARGS__)
#define JNI_FN(name) Java_io_qxengine_core_QXCoreRuntimeBridge_##name

static const char* jstring_to_cstr(JNIEnv* env, jstring js, const char** out)
{
    if (!js) { *out = nullptr; return nullptr; }
    *out = env->GetStringUTFChars(js, nullptr);
    return *out;
}

static void jstring_release(JNIEnv* env, jstring js, const char* cs)
{
    if (js && cs) env->ReleaseStringUTFChars(js, cs);
}

static void throw_exception(JNIEnv* env, const char* msg)
{
    jclass cls = env->FindClass("java/lang/IllegalStateException");
    if (cls) env->ThrowNew(cls, msg);
    LOGE("QXEngine exception: %s", msg);
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*)
{
    (void)vm;
    LOGI("QXEngine JNI bridge loaded ABI=%d", QX_ABI_VERSION);
    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT jlong JNICALL
JNI_FN(nativeEngineCreate)(JNIEnv* env, jobject, jstring manifestJson,
                           jlong deviceRamBytes, jboolean verifyIntegrity,
                           jboolean verbose)
{
    const char* json = nullptr;
    jstring_to_cstr(env, manifestJson, &json);
    if (!json) {
        throw_exception(env, "manifestJson must not be null");
        return 0L;
    }

    QXEngineConfig cfg{};
    cfg.manifest_json = json;
    cfg.manifest_json_length = static_cast<uint32_t>(std::strlen(json));
    cfg.device_ram_bytes = static_cast<uint64_t>(deviceRamBytes);
    cfg.verify_integrity = verifyIntegrity ? QX_TRUE : QX_FALSE;
    cfg.verbose = verbose ? QX_TRUE : QX_FALSE;

    QXEngineHandle engine = nullptr;
    const QXResult rc = qx_engine_create(&cfg, &engine);
    jstring_release(env, manifestJson, json);

    if (rc != QX_OK || !engine) {
        throw_exception(env, "qx_engine_create failed");
        return 0L;
    }

    return reinterpret_cast<jlong>(engine);
}

extern "C" JNIEXPORT jint JNICALL
JNI_FN(nativeEngineStart)(JNIEnv*, jobject, jlong handle)
{
    return qx_engine_start(reinterpret_cast<QXEngineHandle>(handle));
}

extern "C" JNIEXPORT jint JNICALL
JNI_FN(nativeEngineStop)(JNIEnv*, jobject, jlong handle)
{
    return qx_engine_stop(reinterpret_cast<QXEngineHandle>(handle));
}

extern "C" JNIEXPORT void JNICALL
JNI_FN(nativeEngineDestroy)(JNIEnv*, jobject, jlong handle)
{
    qx_engine_destroy(reinterpret_cast<QXEngineHandle>(handle));
}

extern "C" JNIEXPORT jint JNICALL
JNI_FN(nativeNotifyMemoryPressure)(JNIEnv*, jobject, jlong handle, jint level)
{
    return qx_engine_notify_memory_pressure(
        reinterpret_cast<QXEngineHandle>(handle),
        static_cast<uint8_t>(level)
    );
}

extern "C" JNIEXPORT jint JNICALL
JNI_FN(nativeFeedMemoryReading)(JNIEnv*, jobject, jlong handle,
                                jlong residentBytes, jlong systemTotal,
                                jlong systemAvailable)
{
    return qx_engine_feed_memory_reading(
        reinterpret_cast<QXEngineHandle>(handle),
        static_cast<uint64_t>(residentBytes),
        static_cast<uint64_t>(systemTotal),
        static_cast<uint64_t>(systemAvailable)
    );
}

extern "C" JNIEXPORT jint JNICALL
JNI_FN(nativeUpdateBatteryDrain)(JNIEnv*, jobject, jlong handle, jdouble pct)
{
    return qx_engine_update_battery_drain(
        reinterpret_cast<QXEngineHandle>(handle),
        static_cast<double>(pct)
    );
}

extern "C" JNIEXPORT jint JNICALL
JNI_FN(nativeUpdateNetworkRedundancy)(JNIEnv*, jobject, jlong handle, jdouble pct)
{
    return qx_engine_update_network_redundancy(
        reinterpret_cast<QXEngineHandle>(handle),
        static_cast<double>(pct)
    );
}

extern "C" JNIEXPORT jint JNICALL
JNI_FN(nativeMarkCapabilityActive)(JNIEnv* env, jobject, jlong handle,
                                   jstring capabilityId)
{
    const char* capability = nullptr;
    jstring_to_cstr(env, capabilityId, &capability);
    if (!capability) {
        throw_exception(env, "capabilityId must not be null");
        return QX_ERR_NULL_HANDLE;
    }

    const QXResult rc = qx_engine_mark_capability_active(
        reinterpret_cast<QXEngineHandle>(handle),
        capability
    );
    jstring_release(env, capabilityId, capability);
    return rc;
}

extern "C" JNIEXPORT jint JNICALL
JNI_FN(nativeEvaluateNow)(JNIEnv*, jobject, jlong handle)
{
    return qx_engine_evaluate_now(reinterpret_cast<QXEngineHandle>(handle));
}

extern "C" JNIEXPORT jint JNICALL
JNI_FN(nativeIntegrityStatus)(JNIEnv*, jobject, jlong handle)
{
    QXIntegrityStatus status = QX_INTEGRITY_STATUS_UNKNOWN;
    const QXResult rc = qx_engine_integrity_status(
        reinterpret_cast<QXEngineHandle>(handle),
        &status
    );
    return rc == QX_OK ? static_cast<jint>(status) : static_cast<jint>(rc);
}
