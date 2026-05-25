# =============================================================================
# QXEngine Core – tests/CMakeLists.txt
# Owner : Masa Bayu
# Created: 2026-05-24
# Repo   : https://github.com/qxengine/qxengine-core
# Description: Registers all unit and integration tests via CTest.
#              Requires GoogleTest (fetched automatically if not found).
# =============================================================================

cmake_minimum_required(VERSION 3.21)

# ─────────────────────────────────────────────────────────────────────────────
# GoogleTest
# ─────────────────────────────────────────────────────────────────────────────

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)

# Prevent GoogleTest from overriding our compiler/linker settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

# ─────────────────────────────────────────────────────────────────────────────
# Helper macro – declare one unit test executable
# ─────────────────────────────────────────────────────────────────────────────

macro(qx_add_unit_test TEST_NAME SOURCE_FILE)
    add_executable(${TEST_NAME} ${SOURCE_FILE})

    target_include_directories(${TEST_NAME} PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_BINARY_DIR}/include   # generated qx_version.h
    )

    target_link_libraries(${TEST_NAME} PRIVATE
        qxengine
        GTest::gtest_main
    )

    target_compile_features(${TEST_NAME} PRIVATE cxx_std_17)

    gtest_discover_tests(${TEST_NAME}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        DISCOVERY_TIMEOUT 30
        PROPERTIES        TIMEOUT 30 LABELS unit
    )
endmacro()

# ─────────────────────────────────────────────────────────────────────────────
# Helper macro – declare one integration test executable
# ─────────────────────────────────────────────────────────────────────────────

macro(qx_add_integration_test TEST_NAME SOURCE_FILE)
    add_executable(${TEST_NAME} ${SOURCE_FILE})

    target_include_directories(${TEST_NAME} PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_BINARY_DIR}/include
    )

    target_link_libraries(${TEST_NAME} PRIVATE
        qxengine
        GTest::gtest_main
    )

    target_compile_features(${TEST_NAME} PRIVATE cxx_std_17)

    gtest_discover_tests(${TEST_NAME}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        DISCOVERY_TIMEOUT 30
        PROPERTIES        TIMEOUT 60 LABELS integration
    )
endmacro()

# ─────────────────────────────────────────────────────────────────────────────
# Unit tests
# ─────────────────────────────────────────────────────────────────────────────

qx_add_unit_test(test_qx_leaf
    unit/test_qx_leaf.cpp
)

qx_add_unit_test(test_qx_segment
    unit/test_qx_segment.cpp
)

qx_add_unit_test(test_qx_memloc
    unit/test_qx_memloc.cpp
)

qx_add_unit_test(test_qx_memloc_constitutional
    test_qx_memloc_constitutional.cpp
)

qx_add_unit_test(test_qx_memloc_constitutional_api
    test_qx_memloc_constitutional_api.cpp
)

qx_add_unit_test(test_qx_luman
    test_qx_luman.cpp
)

qx_add_unit_test(test_qx_luman_init
    test_qx_luman_init.cpp
)

qx_add_unit_test(test_qx_pressure
    unit/test_qx_pressure.cpp
)

qx_add_unit_test(test_qx_law_enforcer
    unit/test_qx_law_enforcer.cpp
)

qx_add_unit_test(test_qx_snapshot
    unit/test_qx_snapshot.cpp
)

qx_add_unit_test(test_qx_manifest
    unit/test_qx_manifest.cpp
)

qx_add_unit_test(test_qx_manifest_accessors
    unit/test_qx_manifest_accessors.cpp
)

qx_add_unit_test(test_qx_telemetry
    unit/test_qx_telemetry.cpp
)

# ─────────────────────────────────────────────────────────────────────────────
# Integration tests
# ─────────────────────────────────────────────────────────────────────────────

qx_add_integration_test(test_full_evaluation_cycle
    integration/test_full_evaluation_cycle.cpp
)

qx_add_integration_test(test_qx_engine_constitutional
    integration/test_qx_engine_constitutional.cpp
)

target_include_directories(test_qx_engine_constitutional PRIVATE
    ${PROJECT_SOURCE_DIR}/src/core
)

qx_add_integration_test(test_manifest_validation
    integration/test_manifest_validation.cpp
)

qx_add_integration_test(test_manifest_golden
    integration/test_manifest_golden.cpp
)
