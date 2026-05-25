# ==============================================================================
# platforms/ios/CMakeLists.txt
# QXEngine Core – iOS Platform Bridge
# Repository : https://github.com/qxengine/qxengine-core
# Owner      : Masa Bayu
# Created    : 2026-05-24
# Purpose    : Builds the QXEngine Swift/ObjC bridge static library for iOS.
#              Links qxengine-core and exposes the C ABI to Swift via a
#              bridging header. Produces an XCFramework-ready .a artifact.
# ==============================================================================

cmake_minimum_required(VERSION 3.22)
project(qxengine_ios VERSION 1.0.0 LANGUAGES CXX)

# ------------------------------------------------------------------------------
# iOS platform guard
# ------------------------------------------------------------------------------
if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    message(FATAL_ERROR
        "platforms/ios/CMakeLists.txt must be built targeting iOS. "
        "Pass -DCMAKE_SYSTEM_NAME=iOS and use the Apple CMake toolchain."
    )
endif()

# ------------------------------------------------------------------------------
# iOS deployment target
# ------------------------------------------------------------------------------
if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "Minimum iOS version")
endif()

if(CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS "14.0")
    message(FATAL_ERROR
        "QXEngine requires iOS deployment target >= 14.0. "
        "Set CMAKE_OSX_DEPLOYMENT_TARGET=14.0 or higher."
    )
endif()

# ------------------------------------------------------------------------------
# Architectures – support device (arm64) and simulator (arm64 + x86_64)
# ------------------------------------------------------------------------------
if(NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Target architectures")
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
# Swift bridge static library
# ------------------------------------------------------------------------------
add_library(qxengine_ios_bridge STATIC
    bridge/qx_swift_bridge.cpp
)

# ------------------------------------------------------------------------------
# Include directories
# ------------------------------------------------------------------------------
target_include_directories(qxengine_ios_bridge
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../../include"
        "${CMAKE_CURRENT_SOURCE_DIR}/bridge"
)

# ------------------------------------------------------------------------------
# Link libraries
# ------------------------------------------------------------------------------
target_link_libraries(qxengine_ios_bridge
    PRIVATE
        qxengine::qxengine
)

# ------------------------------------------------------------------------------
# Compile options
# ------------------------------------------------------------------------------
target_compile_features(qxengine_ios_bridge PRIVATE cxx_std_17)

target_compile_definitions(qxengine_ios_bridge
    PRIVATE
        QX_PLATFORM_IOS=1
        QX_ABI_VERSION=1
)

target_compile_options(qxengine_ios_bridge
    PRIVATE
        -Wall
        -Wextra
        -Wno-unused-parameter
        $<$<CONFIG:Release>:-O2>
        $<$<CONFIG:Debug>:-O0 -g>
)

# ------------------------------------------------------------------------------
# Symbol visibility
# ------------------------------------------------------------------------------
set_target_properties(qxengine_ios_bridge PROPERTIES
    CXX_VISIBILITY_PRESET     hidden
    VISIBILITY_INLINES_HIDDEN ON
    XCODE_ATTRIBUTE_ENABLE_BITCODE NO
)

# ------------------------------------------------------------------------------
# Install rules – place .a where xcodebuild can find it for XCFramework
# ------------------------------------------------------------------------------
install(TARGETS qxengine_ios_bridge
    ARCHIVE DESTINATION "lib/ios"
)

install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/../../include/qxengine/qxengine.h"
    DESTINATION "include/qxengine"
)
