# ==============================================================================
# QXEngine Core – CMakeLists.txt
# Owner:       Masa Bayu
# Created:     2026-05-24
# Description: Root CMake build file for the QXEngine C++ core engine.
#              Produces a static library (libqxengine) consumed by:
#              - iOS via XCFramework (C ABI bridge)
#              - Android via NDK (JNI bridge)
#              - CLI via direct linkage
#              C++ standard: C++17
#              Test framework: Google Test
#              Documentation: Doxygen
# Repository:  https://github.com/qxengine/qxengine-core
# ==============================================================================

cmake_minimum_required(VERSION 3.22)

# ------------------------------------------------------------------------------
# Project declaration
# ------------------------------------------------------------------------------
project(
    qxengine
    VERSION      1.0.0
    DESCRIPTION  "QXEngine constitutional memory engine – C++ core"
    HOMEPAGE_URL "https://qxengine.io"
    LANGUAGES    CXX C
)

# ------------------------------------------------------------------------------
# C++ standard – C++17, no extensions, required (not optional)
# ------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

# ------------------------------------------------------------------------------
# Build type default
# ------------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY
        STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# ------------------------------------------------------------------------------
# CMake module path
# ------------------------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ------------------------------------------------------------------------------
# Project options
# ------------------------------------------------------------------------------
option(QX_BUILD_TESTS     "Build unit and integration tests"   ON)
option(QX_BUILD_DOCS      "Generate Doxygen documentation"     OFF)
option(QX_ENABLE_ASAN     "Enable AddressSanitizer"            OFF)
option(QX_ENABLE_TSAN     "Enable ThreadSanitizer"             OFF)
option(QX_PLATFORM_ANDROID "Building for Android NDK"          OFF)
option(QX_PLATFORM_IOS     "Building for iOS XCFramework"      OFF)

# ------------------------------------------------------------------------------
# Include cmake modules
# ------------------------------------------------------------------------------
include(QXEngineVersion)
include(QXEngineCompilerFlags)
include(QXEngineInstall)

# ------------------------------------------------------------------------------
# Core library source files
# ------------------------------------------------------------------------------
set(QX_CORE_SOURCES
    # Core
    src/core/qx_constants.cpp
    src/core/qx_error.cpp
    src/core/qx_engine.cpp
    src/core/qx_engine_eval.cpp
    src/core/qx_engine_integrity.cpp
    src/core/qx_engine_memloc_bridge.cpp

    # Memory
    src/memory/qx_leaf.cpp
    src/memory/qx_segment.cpp
    src/memory/qx_segment_api.cpp
    src/memory/qx_memloc.cpp
    src/memory/qx_luman_peringkat.c
    src/memory/qx_luman_budget.c
    src/memory/qx_memloc_pool.c
    src/memory/qx_memloc_pool_commit.c
    src/memory/qx_memloc_pool_query.c
    src/memory/qx_memloc_flow_state.c
    src/memory/qx_memloc_flow_alloc.c
    src/memory/qx_memloc_flow_budget.c
    src/memory/qx_memloc_flow_trigger.c
    src/memory/qx_memloc_signal_registry.c
    src/memory/qx_memloc_signal_measure.c
    src/memory/qx_memloc_signal_cycle.c
    src/memory/qx_memloc_enforce_cycle.c
    src/memory/qx_memloc_enforce_pressure.c
    src/memory/qx_memloc_enforce_equil.c
    src/memory/qx_memloc_enforce_restore.c
    src/memory/qx_memloc_constitutional.c
    src/memory/qx_memloc_constitutional_report.c
    src/memory/qx_pressure.cpp
    src/memory/qx_pressure_api.cpp

    # Law
    src/law/qx_law_types.cpp
    src/law/qx_law_enforcer_eval.cpp
    src/law/qx_law_enforcer.cpp
    src/law/qx_law_enforcer_history.cpp

    # Intelligence
    src/intelligence/qx_snapshot.cpp

    # Manifest
    src/manifest/qx_manifest.cpp
    src/manifest/qx_manifest_parser.cpp
    src/manifest/qx_manifest_validation.cpp

    # Telemetry
    src/telemetry/qx_telemetry.cpp
)

# ------------------------------------------------------------------------------
# Core library header files (for IDE visibility)
# ------------------------------------------------------------------------------
set(QX_CORE_HEADERS
    include/qxengine/qxengine.h

    include/qxengine/core/qx_types.h
    include/qxengine/core/qx_constants.h
    include/qxengine/core/qx_error.h
    include/qxengine/core/qx_version.h
    src/core/qx_engine_memloc_bridge.h

    include/qxengine/memory/qx_leaf.h
    include/qxengine/memory/qx_segment.h
    include/qxengine/memory/qx_memloc.h
    include/qxengine/memory/qx_luman.h
    include/qxengine/memory/qx_memloc_pool.h
    include/qxengine/memory/qx_memloc_flow.h
    include/qxengine/memory/qx_memloc_signal.h
    include/qxengine/memory/qx_memloc_enforce.h
    include/qxengine/memory/qx_memloc_constitutional.h
    include/qxengine/memory/qx_pressure.h

    include/qxengine/law/qx_law_types.h
    include/qxengine/law/qx_law_report.h
    include/qxengine/law/qx_law_enforcer_types.h
    include/qxengine/law/qx_law_enforcer.h

    include/qxengine/intelligence/qx_snapshot_types.h
    include/qxengine/intelligence/qx_snapshot.h

    include/qxengine/manifest/qx_manifest_types.h
    include/qxengine/manifest/qx_manifest.h

    include/qxengine/telemetry/qx_telemetry_event_types.h
    include/qxengine/telemetry/qx_telemetry_types.h
    include/qxengine/telemetry/qx_telemetry.h
)

# ------------------------------------------------------------------------------
# Static library target
# ------------------------------------------------------------------------------
add_library(qxengine STATIC
    ${QX_CORE_SOURCES}
    ${QX_CORE_HEADERS}
)

# ------------------------------------------------------------------------------
# Include directories
# Public:   consumers of libqxengine see include/
# Private:  internal src/ includes
# ------------------------------------------------------------------------------
target_include_directories(qxengine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ------------------------------------------------------------------------------
# Compiler flags (defined in cmake/QXEngineCompilerFlags.cmake)
# ------------------------------------------------------------------------------
qxengine_apply_compiler_flags(qxengine)

# ------------------------------------------------------------------------------
# Sanitizers (debug/CI only)
# ------------------------------------------------------------------------------
if(QX_ENABLE_ASAN)
    target_compile_options(qxengine PUBLIC -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(qxengine    PUBLIC -fsanitize=address)
endif()

if(QX_ENABLE_TSAN)
    target_compile_options(qxengine PUBLIC -fsanitize=thread)
    target_link_options(qxengine    PUBLIC -fsanitize=thread)
endif()

# ------------------------------------------------------------------------------
# Platform-specific build rules
# ------------------------------------------------------------------------------
if(QX_PLATFORM_ANDROID)
    add_subdirectory(platforms/android)
endif()

if(QX_PLATFORM_IOS)
    add_subdirectory(platforms/ios)
endif()

# ------------------------------------------------------------------------------
# Tests
# ------------------------------------------------------------------------------
if(QX_BUILD_TESTS AND NOT QX_PLATFORM_ANDROID AND NOT QX_PLATFORM_IOS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ------------------------------------------------------------------------------
# Documentation
# ------------------------------------------------------------------------------
if(QX_BUILD_DOCS)
    find_package(Doxygen REQUIRED)
    set(DOXYGEN_PROJECT_NAME        "QXEngine Core")
    set(DOXYGEN_PROJECT_VERSION     ${PROJECT_VERSION})
    set(DOXYGEN_PROJECT_BRIEF       "Constitutional memory engine – C++ core API reference")
    set(DOXYGEN_OUTPUT_DIRECTORY    ${CMAKE_CURRENT_BINARY_DIR}/docs)
    set(DOXYGEN_EXTRACT_ALL         YES)
    set(DOXYGEN_EXTRACT_PRIVATE     NO)
    set(DOXYGEN_GENERATE_HTML       YES)
    set(DOXYGEN_GENERATE_XML        YES)
    set(DOXYGEN_USE_MDFILE_AS_MAINPAGE README.md)
    doxygen_add_docs(qxengine_docs
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
        COMMENT "Generating QXEngine Core API documentation"
    )
endif()

# ------------------------------------------------------------------------------
# Install rules (defined in cmake/QXEngineInstall.cmake)
# ------------------------------------------------------------------------------
qxengine_install_rules(qxengine)

# ------------------------------------------------------------------------------
# Summary
# ------------------------------------------------------------------------------
message(STATUS "")
message(STATUS "QXEngine Core ${PROJECT_VERSION}")
message(STATUS "  Build type   : ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard : ${CMAKE_CXX_STANDARD}")
message(STATUS "  Tests        : ${QX_BUILD_TESTS}")
message(STATUS "  Docs         : ${QX_BUILD_DOCS}")
message(STATUS "  ASan         : ${QX_ENABLE_ASAN}")
message(STATUS "  TSan         : ${QX_ENABLE_TSAN}")
message(STATUS "  Android NDK  : ${QX_PLATFORM_ANDROID}")
message(STATUS "  iOS          : ${QX_PLATFORM_IOS}")
message(STATUS "")
