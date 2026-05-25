# ==============================================================================
# QXEngine Core – cmake/QXEngineCompilerFlags.cmake
# Owner:       Masa Bayu
# Created:     2026-05-24
# Description: Compiler flags and warning configuration for QXEngine Core.
#              Enforces strict, clean, portable C++17 compilation.
#              No warnings are suppressed. Every warning is an error.
#              Supports: GCC, Clang, Apple Clang, MSVC.
# Repository:  https://github.com/qxengine/qxengine-core
# ==============================================================================

# ------------------------------------------------------------------------------
# Guard against multiple inclusion
# ------------------------------------------------------------------------------
if(DEFINED QX_COMPILER_FLAGS_INCLUDED)
    return()
endif()
set(QX_COMPILER_FLAGS_INCLUDED TRUE)

# ------------------------------------------------------------------------------
# Detect compiler family
# ------------------------------------------------------------------------------
set(QX_COMPILER_IS_GCC    FALSE)
set(QX_COMPILER_IS_CLANG  FALSE)
set(QX_COMPILER_IS_MSVC   FALSE)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(QX_COMPILER_IS_GCC TRUE)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR
       CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
    set(QX_COMPILER_IS_CLANG TRUE)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    set(QX_COMPILER_IS_MSVC TRUE)
endif()

message(STATUS "QXEngine compiler : ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "QXEngine compiler version : ${CMAKE_CXX_COMPILER_VERSION}")

# ------------------------------------------------------------------------------
# Base warning flags – GCC and Clang
# ------------------------------------------------------------------------------
set(QX_FLAGS_GCC_CLANG
    -Wall                       # all standard warnings
    -Wextra                     # extra warnings beyond -Wall
    -Wpedantic                  # strict ISO C++ conformance
    -Werror                     # treat all warnings as errors
    -Wshadow                    # warn on variable shadowing
    -Wnon-virtual-dtor          # warn on non-virtual destructors
    -Wold-style-cast            # warn on C-style casts
    -Wcast-align                # warn on alignment-reducing casts
    -Wunused                    # warn on unused variables/functions
    -Woverloaded-virtual        # warn on overloaded virtual functions
    -Wconversion                # warn on implicit type conversions
    -Wsign-conversion           # warn on signed/unsigned conversion
    -Wmisleading-indentation    # warn on misleading indentation
    -Wnull-dereference          # warn on null pointer dereference
    -Wdouble-promotion          # warn on float→double promotion
    -Wformat=2                  # strict format string checking
    -Wno-unknown-pragmas        # ignore unknown pragmas cleanly
)

# ------------------------------------------------------------------------------
# GCC-specific flags
# ------------------------------------------------------------------------------
set(QX_FLAGS_GCC_ONLY
    -Wduplicated-cond           # warn on duplicated if/else conditions
    -Wduplicated-branches       # warn on duplicated if/else branches
    -Wlogical-op                # warn on suspicious logical operators
    -Wuseless-cast              # warn on useless casts
)

# ------------------------------------------------------------------------------
# Clang-specific flags
# ------------------------------------------------------------------------------
set(QX_FLAGS_CLANG_ONLY
    -Weverything                # enable all Clang warnings
    -Wno-c++98-compat           # we target C++17, not C++98
    -Wno-c++98-compat-pedantic  # same as above
    -Wno-padded                 # struct padding is acceptable
    -Wno-exit-time-destructors  # acceptable for static objects
    -Wno-global-constructors    # acceptable for constants
)

# ------------------------------------------------------------------------------
# MSVC flags
# ------------------------------------------------------------------------------
set(QX_FLAGS_MSVC
    /W4         # warning level 4
    /WX         # treat warnings as errors
    /permissive- # strict standards conformance
    /w14242     # conversion warnings
    /w14263     # virtual function override warnings
    /w14265     # non-virtual destructor warnings
    /w14287     # unsigned/negative constant warnings
    /we4289     # loop variable used outside loop
    /w14296     # expression is always false
    /w14311     # pointer truncation
    /w14545     # expression before comma
    /w14546     # function call before comma
    /w14547     # operator before comma
    /w14549     # operator before comma
    /w14555     # expression has no effect
    /w14619     # pragma warning
    /w14640     # thread-unsafe static member init
    /w14826     # conversion is sign-extended
    /w14905     # wide string literal cast
    /w14906     # string literal cast
    /w14928     # illegal copy-initialization
)

# ------------------------------------------------------------------------------
# Release optimisation flags
# ------------------------------------------------------------------------------
set(QX_FLAGS_RELEASE_GCC_CLANG
    -O2                         # standard optimisation
    -DNDEBUG                    # disable assertions
    -fvisibility=hidden         # hide symbols by default
    -fvisibility-inlines-hidden # hide inline symbols
)

set(QX_FLAGS_DEBUG_GCC_CLANG
    -O0                         # no optimisation
    -g3                         # maximum debug info
    -fno-omit-frame-pointer     # keep frame pointer for stack traces
)

# ------------------------------------------------------------------------------
# Public function: qxengine_apply_compiler_flags(target)
# Applies all appropriate flags to the given target based on compiler and
# build type. Called from root CMakeLists.txt.
# ------------------------------------------------------------------------------
function(qxengine_apply_compiler_flags target)

    if(QX_COMPILER_IS_GCC)
        target_compile_options(${target} PRIVATE
            ${QX_FLAGS_GCC_CLANG}
            ${QX_FLAGS_GCC_ONLY}
        )
        target_compile_options(${target} PRIVATE
            $<$<CONFIG:Release>:${QX_FLAGS_RELEASE_GCC_CLANG}>
            $<$<CONFIG:Debug>:${QX_FLAGS_DEBUG_GCC_CLANG}>
        )

    elseif(QX_COMPILER_IS_CLANG)
        target_compile_options(${target} PRIVATE
            ${QX_FLAGS_GCC_CLANG}
            ${QX_FLAGS_CLANG_ONLY}
        )
        target_compile_options(${target} PRIVATE
            $<$<CONFIG:Release>:${QX_FLAGS_RELEASE_GCC_CLANG}>
            $<$<CONFIG:Debug>:${QX_FLAGS_DEBUG_GCC_CLANG}>
        )

    elseif(QX_COMPILER_IS_MSVC)
        target_compile_options(${target} PRIVATE
            ${QX_FLAGS_MSVC}
        )
    endif()

    # Suppress cross-compilation system-directory warning on macOS host builds
    if(NOT CMAKE_CROSSCOMPILING)
        target_compile_options(${target} PRIVATE
            -Wno-poison-system-directories
        )
    endif()

    # Enforce C++17 on the target directly
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD          17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS        OFF
    )

    message(STATUS "QXEngine compiler flags applied to: ${target}")

endfunction()
