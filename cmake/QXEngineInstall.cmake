# ==============================================================================
# QXEngine Core – cmake/QXEngineInstall.cmake
# Owner:       Masa Bayu
# Created:     2026-05-24
# Description: Install rules for the QXEngine C++ core static library.
#              Defines how libqxengine is installed for consumption by:
#              - iOS XCFramework build pipeline
#              - Android NDK CMake integration
#              - CLI direct linkage
#              Follows CMake GNUInstallDirs conventions exactly.
# Repository:  https://github.com/qxengine/qxengine-core
# ==============================================================================

# ------------------------------------------------------------------------------
# Guard against multiple inclusion
# ------------------------------------------------------------------------------
if(DEFINED QX_INSTALL_RULES_INCLUDED)
    return()
endif()
set(QX_INSTALL_RULES_INCLUDED TRUE)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ------------------------------------------------------------------------------
# Install prefix defaults
# Override at configure time with -DCMAKE_INSTALL_PREFIX=...
# ------------------------------------------------------------------------------
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX
        "${CMAKE_BINARY_DIR}/install"
        CACHE PATH "Install prefix" FORCE
    )
endif()

# ------------------------------------------------------------------------------
# Install destination constants
# ------------------------------------------------------------------------------
set(QX_INSTALL_LIBDIR     ${CMAKE_INSTALL_LIBDIR})          # lib/
set(QX_INSTALL_INCLUDEDIR ${CMAKE_INSTALL_INCLUDEDIR})      # include/
set(QX_INSTALL_BINDIR     ${CMAKE_INSTALL_BINDIR})          # bin/
set(QX_INSTALL_CMAKEDIR   ${CMAKE_INSTALL_LIBDIR}/cmake/qxengine)

# ------------------------------------------------------------------------------
# Public function: qxengine_install_rules(target)
# Registers install rules for the static library, headers, and CMake
# package config files. Called from root CMakeLists.txt after target
# definition.
# ------------------------------------------------------------------------------
function(qxengine_install_rules target)

    # ── Library binary ────────────────────────────────────────────────────
    install(
        TARGETS ${target}
        EXPORT  qxengineTargets
        ARCHIVE DESTINATION ${QX_INSTALL_LIBDIR}
            COMPONENT Development
        LIBRARY DESTINATION ${QX_INSTALL_LIBDIR}
            COMPONENT Runtime
        RUNTIME DESTINATION ${QX_INSTALL_BINDIR}
            COMPONENT Runtime
    )

    # ── Public headers ────────────────────────────────────────────────────
    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/qxengine
        DESTINATION ${QX_INSTALL_INCLUDEDIR}
        COMPONENT Development
        FILES_MATCHING
            PATTERN "*.h"
            PATTERN "*.hpp"
    )

    # ── CMake export targets ──────────────────────────────────────────────
    install(
        EXPORT qxengineTargets
        FILE       qxengineTargets.cmake
        NAMESPACE  qxengine::
        DESTINATION ${QX_INSTALL_CMAKEDIR}
        COMPONENT Development
    )

    # ── Package config file ───────────────────────────────────────────────
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/qxengineConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/qxengineConfig.cmake"
        INSTALL_DESTINATION ${QX_INSTALL_CMAKEDIR}
        PATH_VARS
            QX_INSTALL_INCLUDEDIR
            QX_INSTALL_LIBDIR
    )

    # ── Package version file ──────────────────────────────────────────────
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/qxengineConfigVersion.cmake"
        VERSION       ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    # ── Install config and version files ─────────────────────────────────
    install(
        FILES
            "${CMAKE_CURRENT_BINARY_DIR}/qxengineConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/qxengineConfigVersion.cmake"
        DESTINATION ${QX_INSTALL_CMAKEDIR}
        COMPONENT Development
    )

    message(STATUS "QXEngine install rules registered for: ${target}")
    message(STATUS "  Install prefix  : ${CMAKE_INSTALL_PREFIX}")
    message(STATUS "  Library dir     : ${QX_INSTALL_LIBDIR}")
    message(STATUS "  Include dir     : ${QX_INSTALL_INCLUDEDIR}")
    message(STATUS "  CMake dir       : ${QX_INSTALL_CMAKEDIR}")

endfunction()
