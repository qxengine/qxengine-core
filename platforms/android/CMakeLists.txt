# ==============================================================================
# platforms/android/CMakeLists.txt
# QXEngine Core – Android Platform Bridge
# Repository : https://github.com/qxengine/qxengine-core
# Owner      : Masa Bayu
# Created    : 2026-05-24
# Purpose    : Builds the QXEngine JNI shared library (.so) for Android NDK.
#              Links qxengine-core, exposes the C ABI via JNI bridge.
# ==============================================================================

cmake_minimum_required(VERSION 3.22)
project(qxengine_android VERSION 1.0.0 LANGUAGES CXX)

# ------------------------------------------------------------------------------
# Android NDK guard
# ------------------------------------------------------------------------------
if(NOT ANDROID)
    message(FATAL_ERROR
        "platforms/android/CMakeLists.txt must be built with the Android NDK "
        "toolchain. Pass -DCMAKE_TOOLCHAIN_FILE=<ndk>/build/cmake/android.toolchain.cmake"
    )
endif()

# ------------------------------------------------------------------------------
# ABI and API level requirements
# ------------------------------------------------------------------------------
if(ANDROID_ABI STREQUAL "armeabi-v7a")
    message(WARNING "armeabi-v7a is deprecated. Prefer arm64-v8a or x86_64.")
endif()

if(ANDROID_PLATFORM LESS 26)
    message(FATAL_ERROR
        "QXEngine requires Android API level >= 26 (Android 8.0). "
        "Set ANDROID_PLATFORM=android-26 or higher."
    )
endif()

# ------------------------------------------------------------------------------
# Locate qxengine-core
# ------------------------------------------------------------------------------
find_package(qxengine REQUIRED
    HINTS
        "${CMAKE_CURRENT_SOURCE_DIR}/../../build"
        "${CMAKE_CURRENT_SOURCE_DIR}/../.."
)

# ------------------------------------------------------------------------------
# JNI bridge shared library
# ------------------------------------------------------------------------------
add_library(qxengine_jni SHARED
    jni/qx_jni_bridge.cpp
)

# ------------------------------------------------------------------------------
# Include directories
# ------------------------------------------------------------------------------
target_include_directories(qxengine_jni
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../../include"
        "${CMAKE_CURRENT_SOURCE_DIR}/jni"
)

# ------------------------------------------------------------------------------
# Link libraries
# ------------------------------------------------------------------------------
find_library(ANDROID_LOG_LIB  log)
find_library(ANDROID_LIB      android)

target_link_libraries(qxengine_jni
    PRIVATE
        qxengine::qxengine
        ${ANDROID_LOG_LIB}
        ${ANDROID_LIB}
)

# ------------------------------------------------------------------------------
# Compile options
# ------------------------------------------------------------------------------
target_compile_features(qxengine_jni PRIVATE cxx_std_17)

target_compile_definitions(qxengine_jni
    PRIVATE
        QX_PLATFORM_ANDROID=1
        QX_ABI_VERSION=1
)

target_compile_options(qxengine_jni
    PRIVATE
        -Wall
        -Wextra
        -Wno-unused-parameter
        $<$<CONFIG:Release>:-O2>
        $<$<CONFIG:Debug>:-O0 -g>
)

# ------------------------------------------------------------------------------
# Symbol visibility – strip unused symbols in release
# ------------------------------------------------------------------------------
set_target_properties(qxengine_jni PROPERTIES
    CXX_VISIBILITY_PRESET     hidden
    VISIBILITY_INLINES_HIDDEN ON
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_link_options(qxengine_jni PRIVATE -Wl,--strip-all)
endif()

# ------------------------------------------------------------------------------
# Install rules
# ------------------------------------------------------------------------------
install(TARGETS qxengine_jni
    LIBRARY DESTINATION "lib/${ANDROID_ABI}"
)

