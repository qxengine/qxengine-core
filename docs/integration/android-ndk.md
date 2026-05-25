# QXEngine Core — Integration Guide: Android NDK

**Repository:** https://github.com/qxengine/qxengine-core
**Owner:** Masa Bayu
**Created:** 2026-05-24

---

## Overview

This guide explains how to integrate QXEngine Core into an Android
application using the Android NDK and CMake. The JNI bridge
(`qx_jni_bridge.cpp`) exposes the full engine C ABI to Kotlin and Java
callers via a thin JNI layer.

---

## Prerequisites

| Requirement | Minimum version |
|---|---|
| Android NDK | r25c (25.2.9519653) |
| CMake | 3.22 |
| Android API level | 26 (Android 8.0) |
| Android Gradle Plugin | 7.4 |
| ABI filters | `arm64-v8a`, `x86_64` |

---

## Recommended Project Structure

```
MyApp/
├── app/
│   ├── src/main/
│   │   ├── cpp/
│   │   │   └── CMakeLists.txt        ← links qxengine_jni
│   │   ├── java/io/qxengine/core/
│   │   │   └── QXEngine.kt           ← Kotlin wrapper
│   │   └── assets/
│   │       └── qxengine_manifest.json
│   └── build.gradle
└── qxengine-core/                    ← this repository (git submodule)
    └── platforms/android/
        ├── CMakeLists.txt
        └── jni/qx_jni_bridge.cpp
```

---

## CMake Integration

In your app's `app/src/main/cpp/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.22)
project(myapp_native)

# Add qxengine-core as a subdirectory
add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../qxengine-core
    ${CMAKE_CURRENT_BINARY_DIR}/qxengine-core
)

# Add the JNI bridge
add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../qxengine-core/platforms/android
    ${CMAKE_CURRENT_BINARY_DIR}/qxengine-android
)

# Link your own native library against qxengine_jni
add_library(myapp_native SHARED myapp_native.cpp)
target_link_libraries(myapp_native PRIVATE qxengine_jni)
```

---

## Gradle Integration

In `app/build.gradle`:

```groovy
android {
    defaultConfig {
        ndk {
            abiFilters "arm64-v8a", "x86_64"
        }
        externalNativeBuild {
            cmake {
                cppFlags "-std=c++17"
                arguments "-DANDROID_STL=c++_shared",
                          "-DANDROID_PLATFORM=android-26"
            }
        }
    }

    externalNativeBuild {
        cmake {
            path "src/main/cpp/CMakeLists.txt"
            version "3.22.1"
        }
    }
}

dependencies {
    implementation 'androidx.core:core-ktx:1.12.0'
}
```

---

## Loading the Manifest

Store your manifest JSON in `assets/qxengine_manifest.json`. Load it
from Kotlin before calling `nativeEngineCreate`:

```kotlin
class QXEngine(context: Context) {

    private var handle: Long = 0L

    init {
        System.loadLibrary("qxengine_jni")
        val json = context.assets
            .open("qxengine_manifest.json")
            .bufferedReader()
            .readText()
        handle = nativeEngineCreate(json)
        check(handle != 0L) { "QXEngine: failed to create engine" }
    }

    fun destroy() {
        if (handle != 0L) {
            nativeEngineDestroy(handle)
            handle = 0L
        }
    }

    fun evaluate(): Double = nativeEngineEvaluate(handle)
    fun snapshot(): Double = nativeEngineSnapshot(handle)
    fun stats(): LongArray = nativeEngineStats(handle) ?: LongArray(4)

    fun onTrimMemory(level: Int) {
        if (handle != 0L) nativeEnginePressureTrim(handle, level)
    }

    // Native declarations
    private external fun nativeEngineCreate(manifestJson: String): Long
    private external fun nativeEngineDestroy(handle: Long)
    private external fun nativeEngineAlloc(handle: Long, segmentId: Int,
                                            sizeBytes: Long, label: String,
                                            evictClass: Int): Long
    private external fun nativeEngineFree(handle: Long, leaf: Long): Boolean
    private external fun nativeEngineEvaluate(handle: Long): Double
    private external fun nativeEngineSnapshot(handle: Long): Double
    private external fun nativeEngineStats(handle: Long): LongArray?
    private external fun nativeEnginePressureTrim(handle: Long,
                                                   trimLevel: Int): Boolean
}
```

---

## Handling Memory Pressure

Override `onTrimMemory` in your `Application` or `Activity` class and
forward the trim level to QXEngine:

```kotlin
override fun onTrimMemory(level: Int) {
    super.onTrimMemory(level)
    qxEngine.onTrimMemory(level)
}
```

QXEngine maps Android trim levels to pressure tiers automatically:

| `onTrimMemory` level | QXEngine tier |
|---|---|
| `TRIM_MEMORY_RUNNING_MODERATE` (5) | Tier 2 — Elevated |
| `TRIM_MEMORY_RUNNING_LOW` (10) | Tier 3 — High |
| `TRIM_MEMORY_RUNNING_CRITICAL` (15) | Tier 4 — Critical |
| Any other level | Tier 2 — Elevated |

---

## ProGuard / R8 Rules

Add the following to your `proguard-rules.pro` to prevent stripping
of the JNI bridge class:

```proguard
-keep class io.qxengine.core.QXEngine { *; }
-keepclasseswithmembernames class * {
    native <methods>;
}
```

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| `UnsatisfiedLinkError` on `loadLibrary` | Wrong ABI filter or missing `.so` | Confirm `abiFilters` includes device ABI |
| `IllegalStateException: Manifest validation failed` | Invalid manifest JSON | Check MFT rules in `docs/api/manifest.md` |
| Engine handle returns 0 | Manifest parse error | Log `error_count` from validation result |
| Crash on `nativeEngineDestroy` | Double-destroy | Null handle after destroy in Kotlin wrapper |
| High memory pressure at tier 4 | Segment over hard limit | Review segment `budget_percent` in manifest |

---

## Related Documents

- [`../architecture/overview.md`](../architecture/overview.md) — Engine architecture
- [`../api/manifest.md`](../api/manifest.md) — Manifest format and MFT rules
- [`../api/memory.md`](../api/memory.md) — Memory pressure API
- [`ios-xcframework.md`](ios-xcframework.md) — iOS integration guide
- [`../../platforms/android/jni/qx_jni_bridge.cpp`](../../platforms/android/jni/qx_jni_bridge.cpp) — JNI bridge source
