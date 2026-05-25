# ==============================================================================
# QXEngine Core – cmake/QXEngineVersion.cmake
# Owner:       Masa Bayu
# Created:     2026-05-24
# Description: Version constants for the QXEngine C++ core engine.
#              All version strings are derived from this single file.
#              Any version bump must happen here and nowhere else.
# Repository:  https://github.com/qxengine/qxengine-core
# ==============================================================================

# ------------------------------------------------------------------------------
# Canonical version components
# Must match project() declaration in root CMakeLists.txt
# ------------------------------------------------------------------------------
set(QX_VERSION_MAJOR 1)
set(QX_VERSION_MINOR 0)
set(QX_VERSION_PATCH 0)
set(QX_VERSION_TWEAK 0)

# ------------------------------------------------------------------------------
# Composed version strings
# ------------------------------------------------------------------------------
set(QX_VERSION
    "${QX_VERSION_MAJOR}.${QX_VERSION_MINOR}.${QX_VERSION_PATCH}")

set(QX_VERSION_FULL
    "${QX_VERSION_MAJOR}.${QX_VERSION_MINOR}.${QX_VERSION_PATCH}.${QX_VERSION_TWEAK}")

set(QX_VERSION_STRING
    "QXEngine Core ${QX_VERSION}")

# ------------------------------------------------------------------------------
# Spec and schema version
# Must match qxengine-spec and qxengine-examples manifests
# ------------------------------------------------------------------------------
set(QX_SPEC_VERSION   "1.0.0")
set(QX_SCHEMA_URL     "https://qxengine.io/manifest/v1.0/schema.json")

# ------------------------------------------------------------------------------
# Owner – must appear in every generated file header
# ------------------------------------------------------------------------------
set(QX_OWNER          "Masa Bayu")
set(QX_CREATED        "2026-05-24")
set(QX_HOMEPAGE       "https://qxengine.io")
set(QX_REPOSITORY     "https://github.com/qxengine/qxengine-core")

# ------------------------------------------------------------------------------
# ABI version
# Increment SOVERSION when the C ABI boundary changes.
# ABI changes require a major version bump.
# ------------------------------------------------------------------------------
set(QX_ABI_VERSION    1)
set(QX_SOVERSION      ${QX_ABI_VERSION})

# ------------------------------------------------------------------------------
# Configure version header from template
# Generates include/qxengine/core/qx_version.h at configure time
# ------------------------------------------------------------------------------
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/qx_version.h.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/qxengine/core/qx_version.h"
    @ONLY
)

# ------------------------------------------------------------------------------
# Print version at configure time
# ------------------------------------------------------------------------------
message(STATUS "QXEngine version  : ${QX_VERSION}")
message(STATUS "QXEngine spec     : ${QX_SPEC_VERSION}")
message(STATUS "QXEngine ABI      : ${QX_ABI_VERSION}")
message(STATUS "QXEngine owner    : ${QX_OWNER}")
message(STATUS "QXEngine schema   : ${QX_SCHEMA_URL}")
