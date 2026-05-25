// =============================================================================
// QXEngine Core – src/core/qx_error.cpp
// Owner: Masa Bayu
// Created: 2026-05-24
// Description: Implementation of qx_error_string() – the full error code
//              registry mapping every QXResult code to a human-readable
//              description string.
//
// Design constraints:
//   • No heap allocation.
//   • Linear table scan – acceptable as this is an error path only.
//   • All strings are string literals (static storage duration).
//   • C linkage preserved via extern "C".
//
// Repository: https://github.com/qxengine/qxengine-core
// =============================================================================

#include "qxengine/core/qx_error.h"
#include "qxengine/core/qx_types.h"

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Error string table (internal linkage)
// ─────────────────────────────────────────────────────────────────────────────

namespace {

struct ErrorEntry {
    QXResult    code;
    const char *description;
};

/**
 * @brief Full registry of every QXResult error code and its description.
 *
 * Grouped by error range to match the range definitions in qx_error.h:
 *   0            Success
 *   -1   – -4    General
 *   -1001– -1010 Manifest
 *   -2001– -2012 Memory
 *   -3001– -3008 Constitutional law
 *   -4001– -4004 Engine lifecycle
 *   -5001– -5003 Telemetry
 *   -6001– -6003 Security
 *   -9999        Internal
 */
static const ErrorEntry kErrorTable[] = {

    // ── Success ───────────────────────────────────────────────────────────────
    {
        QX_OK,
        "Success"
    },

    // ── General ───────────────────────────────────────────────────────────────
    {
        QX_ERR_NULL_HANDLE,
        "Null handle or required pointer argument"
    },
    {
        QX_ERR_BUFFER_TOO_SMALL,
        "Provided buffer is too small for the output"
    },
    {
        QX_ERR_NOT_SUPPORTED,
        "Operation is not supported in this configuration"
    },
    {
        QX_ERR_INVALID_ARGUMENT,
        "One or more arguments are invalid"
    },

    // ── Manifest ──────────────────────────────────────────────────────────────
    {
        QX_ERR_MANIFEST_NOT_FOUND,
        "Manifest file not found"
    },
    {
        QX_ERR_MANIFEST_UNREADABLE,
        "Manifest file could not be read"
    },
    {
        QX_ERR_MANIFEST_PARSE,
        "Manifest JSON is malformed or unparseable"
    },
    {
        QX_ERR_MANIFEST_INVALID,
        "Manifest failed constitutional validation (MFT)"
    },
    {
        QX_ERR_MANIFEST_SCHEMA_MISMATCH,
        "Manifest schema URL does not match expected value"
    },
    {
        QX_ERR_MANIFEST_INVALID_APP_ID,
        "Manifest identity.appId is blank or too long"
    },
    {
        QX_ERR_MANIFEST_BUDGET_SUM,
        "Manifest segment budgetPercent values do not sum to 100"
    },
    {
        QX_ERR_MANIFEST_SEGMENT_COUNT,
        "Manifest does not declare exactly 9 segments"
    },
    {
        QX_ERR_MANIFEST_LIMIT_TOO_LOW,
        "Manifest memory limits are below minimum thresholds"
    },
    {
        QX_ERR_MANIFEST_LAW_INACTIVE,
        "One or more constitutional law flags are inactive"
    },

    // ── Memory ────────────────────────────────────────────────────────────────
    {
        QX_ERR_LABEL_BLANK,
        "Leaf label is blank or whitespace-only (Law 1)"
    },
    {
        QX_ERR_LABEL_TOO_SHORT,
        "Leaf label is shorter than 3 characters (Law 1)"
    },
    {
        QX_ERR_LABEL_TOO_LONG,
        "Leaf label exceeds 128 characters (Law 1)"
    },
    {
        QX_ERR_UNKNOWN_SEGMENT,
        "Segment ID is not recognised by this engine instance"
    },
    {
        QX_ERR_NO_SLOT,
        "No allocation slot available in segment (Law 2)"
    },
    {
        QX_ERR_SOFT_BUDGET,
        "Allocation would exceed segment soft budget (Law 2)"
    },
    {
        QX_ERR_HARD_BUDGET,
        "Allocation would exceed segment hard budget (Law 2)"
    },
    {
        QX_ERR_LEAF_NOT_FOUND,
        "Leaf ID not found in any segment"
    },
    {
        QX_ERR_DOUBLE_FREE,
        "Leaf has already been deallocated (Law 3)"
    },
    {
        QX_ERR_PROTECTED_EVICTION,
        "Class-A leaf cannot be evicted at any pressure tier"
    },
    {
        QX_ERR_INSUFFICIENT_EVICTION,
        "Eviction cycle did not achieve pressure relief"
    },
    {
        QX_ERR_INVALID_LEAF_CLASS,
        "Leaf class value is not in range A–D"
    },

    // ── Constitutional law ────────────────────────────────────────────────────
    {
        QX_ERR_LAW_PATTERN,
        "Constitutional Law 1 (Pattern) violation"
    },
    {
        QX_ERR_LAW_LIMIT,
        "Constitutional Law 2 (Limit) violation"
    },
    {
        QX_ERR_LAW_PAIRS,
        "Constitutional Law 3 (Pairs) violation"
    },
    {
        QX_ERR_LAW_EQUILIBRIUM,
        "Constitutional Law 4 (Equilibrium) violation"
    },
    {
        QX_ERR_LAW_KNOWLEDGE,
        "Constitutional Law 5 (Knowledge) violation"
    },
    {
        QX_ERR_LAW_ETHICS,
        "Constitutional Law 6 (Ethics) violation"
    },
    {
        QX_ERR_LAW_CREATIVITY,
        "Constitutional Law 7 (Creativity) violation"
    },
    {
        QX_ERR_LAW_ECONOMY,
        "Constitutional Law 8 (Economy) violation"
    },

    // ── Engine lifecycle ──────────────────────────────────────────────────────
    {
        QX_ERR_NOT_READY,
        "Engine is not in a ready state for this operation"
    },
    {
        QX_ERR_ALREADY_RUNNING,
        "Engine is already running; cannot start again"
    },
    {
        QX_ERR_ALREADY_STOPPED,
        "Engine is already stopped"
    },
    {
        QX_ERR_NOT_CONFIGURED,
        "Engine has not been configured with a manifest"
    },

    // ── Telemetry ─────────────────────────────────────────────────────────────
    {
        QX_ERR_TELEMETRY_WRITE,
        "Telemetry event could not be written to the log"
    },
    {
        QX_ERR_TELEMETRY_REPORT,
        "Telemetry report generation failed"
    },
    {
        QX_ERR_KNOWLEDGE_SCORE_LOW,
        "Knowledge score is below the Law 5 minimum threshold"
    },

    // ── Security ──────────────────────────────────────────────────────────────
    {
        QX_ERR_INTEGRITY_MISMATCH,
        "Integrity check failed: manifest or state was tampered"
    },
    {
        QX_ERR_ENCRYPTION_FAILED,
        "Encryption operation failed"
    },
    {
        QX_ERR_DECRYPTION_FAILED,
        "Decryption operation failed"
    },

    // ── Internal ──────────────────────────────────────────────────────────────
    {
        QX_ERR_INTERNAL,
        "Unexpected internal engine error"
    }
};

static const uint32_t kErrorTableSize =
    static_cast<uint32_t>(sizeof(kErrorTable) / sizeof(kErrorTable[0]));

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Public C ABI
// ─────────────────────────────────────────────────────────────────────────────

extern "C" {

/**
 * @brief Return a human-readable description for a QXResult error code.
 *
 * Performs a linear scan of kErrorTable. Acceptable cost: this function
 * is called only on error paths, never in the hot allocation loop.
 *
 * @param result  Any QXResult value.
 * @return Null-terminated description string. Never returns NULL.
 *
 * @note Thread-safe. Accesses only static read-only data.
 */
QX_API const char *qx_error_string(QXResult result) {
    for (uint32_t i = 0u; i < kErrorTableSize; ++i) {
        if (kErrorTable[i].code == result) {
            return kErrorTable[i].description;
        }
    }
    return "Unknown error code";
}

} // extern "C"

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Compile-time invariant checks
// ─────────────────────────────────────────────────────────────────────────────

namespace {

/**
 * @brief Verify the error table covers the expected number of entries.
 *
 * Current count:
 *   1  success
 *   4  general
 *   10 manifest
 *   12 memory
 *   8  constitutional law
 *   4  lifecycle
 *   3  telemetry
 *   3  security
 *   1  internal
 *   ──
 *   46 total
 */
static_assert(
    kErrorTableSize == 46u,
    "kErrorTable entry count mismatch – update this assert when adding codes"
);

} // anonymous namespace
