/* =============================================================================
 * platforms/android/jni/qx_jni_bridge.cpp
 * QXEngine Core – Android JNI Bridge
 * Repository : https://github.com/qxengine/qxengine-core
 * Owner      : Masa Bayu
 * Created    : 2026-05-24
 * Purpose    : Exposes the QXEngine C ABI to the Android JVM via JNI.
 *              Each native method maps 1-to-1 to a qx_engine_* function.
 *              The engine handle is stored as a jlong on the Java side.
 * =========================================================================== */
#include <jni.h>
#include <android/log.h>
#include <cstring>
#include <cstdint>
#include <cstdlib>

#include "qxengine/qxengine.h"
#include "qxengine/manifest/qx_manifest.h"
#include "qxengine/law/qx_law_report.h"
#include "qxengine/intelligence/qx_snapshot_types.h"

/* -------------------------------------------------------------------------- */
/* Macros                                                                      */
/* -------------------------------------------------------------------------- */
#define QX_LOG_TAG   "QXEngine"
#define QX_PKG       "io/qxengine/core/QXEngine"
#define LOGI(...)    __android_log_print(ANDROID_LOG_INFO,  QX_LOG_TAG, __VA_ARGS__)
#define LOGE(...)    __android_log_print(ANDROID_LOG_ERROR, QX_LOG_TAG, __VA_ARGS__)
#define JNI_FN(name) Java_io_qxengine_core_QXEngine_##name

/* -------------------------------------------------------------------------- */
/* Helper – convert jstring to a transient C string (must ReleaseStringUTFChars) */
/* -------------------------------------------------------------------------- */
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

/* -------------------------------------------------------------------------- */
/* Helper – throw a Java IllegalStateException                                 */
/* -------------------------------------------------------------------------- */
static void throw_exception(JNIEnv* env, const char* msg)
{
    jclass cls = env->FindClass("java/lang/IllegalStateException");
    if (cls) env->ThrowNew(cls, msg);
    LOGE("QXEngine exception: %s", msg);
}

/* -------------------------------------------------------------------------- */
/* JNI_OnLoad                                                                  */
/* -------------------------------------------------------------------------- */
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* /*reserved*/)
{
    (void)vm;
    LOGI("QXEngine JNI bridge loaded (ABI v%d)", QX_ABI_VERSION);
    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* /*reserved*/)
{
    (void)vm;
    LOGI("QXEngine JNI bridge unloaded");
}

/* -------------------------------------------------------------------------- */
/* nativeEngineCreate                                                          */
/* Signature: (Ljava/lang/String;)J                                            */
/* Returns engine handle as jlong, or 0 on failure.                           */
/* -------------------------------------------------------------------------- */
extern "C" JNIEXPORT jlong JNICALL
JNI_FN(nativeEngineCreate)(JNIEnv* env, jobject /*thiz*/, jstring manifestJson)
{
    const char* json = nullptr;
    jstring_to_cstr(env, manifestJson, &json);
    if (!json) {
        throw_exception(env, "manifestJson must not be null");
        return 0L;
    }

    QXManifestHandle   manifest = nullptr;
    QXManifestValidationResult result;
    std::memset(&result, 0, sizeof(result));

    QXError err = qx_manifest_parse(json, &manifest, &result);
    jstring_release(env, manifestJson, json);

    if (err != QX_OK || !result.is_valid) {
        LOGE("Manifest parse failed: error_count=%u", result.error_count);
        throw_exception(env, "Manifest validation failed");
        return 0L;
    }

    QXEngineConfig cfg;
    std::memset(&cfg, 0, sizeof(cfg));
    cfg.manifest = manifest;

    QXEngineHandle engine = nullptr;
    err = qx_engine_create(&cfg, &engine);
    if (err != QX_OK || !engine) {
        qx_manifest_destroy(manifest);
        throw_exception(env, "qx_engine_create failed");
        return 0L;
    }

    LOGI("Engine created handle=%p", static_cast<void*>(engine));
    return reinterpret_cast<jlong>(engine);
}

/* -------------------------------------------------------------------------- */
/* nativeEngineDestroy                                                         */
/* Signature: (J)V                                                             */
/* -------------------------------------------------------------------------- */
extern "C" JNIEXPORT void JNICALL
JNI_FN(nativeEngineDestroy)(JNIEnv* env, jobject /*thiz*/, jlong handle)
{
    auto* engine = reinterpret_cast<QXEngineHandle>(handle);
    if (!engine) {
        throw_exception(env, "Engine handle is null");
        return;
    }
    qx_engine_destroy(engine);
    LOGI("Engine destroyed handle=%p", static_cast<void*>(engine));
}

/* -------------------------------------------------------------------------- */
/* nativeEngineAlloc                                                           */
/* Signature: (JIJLjava/lang/String;I)J                                        */
/* Returns leaf handle as jlong, or 0 on failure.                             */
/* -------------------------------------------------------------------------- */
extern "C" JNIEXPORT jlong JNICALL
JNI_FN(nativeEngineAlloc)(JNIEnv* env, jobject /*thiz*/,
                           jlong handle, jint segmentId,
                           jlong sizeBytes, jstring label, jint evictClass)
{
    auto* engine = reinterpret_cast<QXEngineHandle>(handle);
    if (!engine) { throw_exception(env, "Engine handle is null"); return 0L; }

    const char* lbl = nullptr;
    jstring_to_cstr(env, label, &lbl);

    QXLeafHandle leaf = nullptr;
    QXError err = qx_engine_alloc(engine,
                                  static_cast<uint8_t>(segmentId),
                                  static_cast<size_t>(sizeBytes),
                                  lbl ? lbl : "",
                                  static_cast<QXEvictClass>(evictClass),
                                  &leaf);
    jstring_release(env, label, lbl);

    if (err != QX_OK || !leaf) {
        LOGE("qx_engine_alloc failed err=%d", err);
        return 0L;
    }
    return reinterpret_cast<jlong>(leaf);
}

/* -------------------------------------------------------------------------- */
/* nativeEngineFree                                                            */
/* Signature: (JJ)Z                                                            */
/* -------------------------------------------------------------------------- */
extern "C" JNIEXPORT jboolean JNICALL
JNI_FN(nativeEngineFree)(JNIEnv* env, jobject /*thiz*/,
                          jlong handle, jlong leafHandle)
{
    auto* engine = reinterpret_cast<QXEngineHandle>(handle);
    auto* leaf   = reinterpret_cast<QXLeafHandle>(leafHandle);
    if (!engine || !leaf) { throw_exception(env, "Null handle"); return JNI_FALSE; }
    return qx_engine_free(engine, leaf) == QX_OK ? JNI_TRUE : JNI_FALSE;
}

/* -------------------------------------------------------------------------- */
/* nativeEngineEvaluate                                                        */
/* Signature: (J)D  — returns health score, or -1.0 on failure                */
/* -------------------------------------------------------------------------- */
extern "C" JNIEXPORT jdouble JNICALL
JNI_FN(nativeEngineEvaluate)(JNIEnv* env, jobject /*thiz*/, jlong handle)
{
    auto* engine = reinterpret_cast<QXEngineHandle>(handle);
    if (!engine) { throw_exception(env, "Engine handle is null"); return -1.0; }

    QXLawReport report;
    std::memset(&report, 0, sizeof(report));
    if (qx_engine_evaluate(engine, &report) != QX_OK) return -1.0;

    LOGI("Evaluate health_score=%.2f certified=%d",
         report.health_score, qx_report_is_certified(&report));
    return static_cast<jdouble>(report.health_score);
}

/* -------------------------------------------------------------------------- */
/* nativeEngineSnapshot                                                        */
/* Signature: (J)D  — returns knowledge score, or -1.0 on failure             */
/* -------------------------------------------------------------------------- */
extern "C" JNIEXPORT jdouble JNICALL
JNI_FN(nativeEngineSnapshot)(JNIEnv* env, jobject /*thiz*/, jlong handle)
{
    auto* engine = reinterpret_cast<QXEngineHandle>(handle);
    if (!engine) { throw_exception(env, "Engine handle is null"); return -1.0; }

    QXCognitiveSnapshot snap;
    std::memset(&snap, 0, sizeof(snap));
    if (qx_engine_snapshot(engine, &snap) != QX_OK) return -1.0;

    return static_cast<jdouble>(snap.knowledge_score);
}

/* -------------------------------------------------------------------------- */
/* nativeEngineStats                                                           */
/* Signature: (J)[J  — returns long[] {alloc_count, free_count, eval_count,   */
/*                                      snapshot_count}                        */
/* -------------------------------------------------------------------------- */
extern "C" JNIEXPORT jlongArray JNICALL
JNI_FN(nativeEngineStats)(JNIEnv* env, jobject /*thiz*/, jlong handle)
{
    auto* engine = reinterpret_cast<QXEngineHandle>(handle);
    if (!engine) { throw_exception(env, "Engine handle is null"); return nullptr; }

    QXEngineStats stats;
    std::memset(&stats, 0, sizeof(stats));
    if (qx_engine_stats(engine, &stats) != QX_OK) return nullptr;

    jlongArray arr = env->NewLongArray(4);
    if (!arr) return nullptr;
    jlong buf[4] = {
        static_cast<jlong>(stats.alloc_count),
        static_cast<jlong>(stats.free_count),
        static_cast<jlong>(stats.evaluation_count),
        static_cast<jlong>(stats.snapshot_count)
    };
    env->SetLongArrayRegion(arr, 0, 4, buf);
    return arr;
}

/* -------------------------------------------------------------------------- */
/* nativeEnginePressureTrim                                                    */
/* Signature: (JI)Z                                                            */
/* -------------------------------------------------------------------------- */
extern "C" JNIEXPORT jboolean JNICALL
JNI_FN(nativeEnginePressureTrim)(JNIEnv* env, jobject /*thiz*/,
                                  jlong handle, jint trimLevel)
{
    auto* engine = reinterpret_cast<QXEngineHandle>(handle);
    if (!engine) { throw_exception(env, "Engine handle is null"); return JNI_FALSE; }
    LOGI("Android trim level=%d", trimLevel);
    return qx_engine_pressure_trim(engine, trimLevel) == QX_OK
           ? JNI_TRUE : JNI_FALSE;
}
