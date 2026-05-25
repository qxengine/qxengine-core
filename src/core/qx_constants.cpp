// =============================================================================
// QXEngine Core – src/core/qx_constants.cpp
// Owner: Masa Bayu
// Created: 2026-05-24
// Description: Runtime constant accessors – segment ID registry, law names,
//              severity names, certification helpers, and static assertions.
//              Error string table lives in src/core/qx_error.cpp.
//
// Design constraints:
//   • No heap allocation.
//   • All string tables statically allocated.
//   • All functions are pure or access only static data.
//   • C linkage preserved via extern "C".
//
// Repository: https://github.com/qxengine/qxengine-core
// =============================================================================

#include "qxengine/core/qx_constants.h"
#include "qxengine/core/qx_types.h"
#include "qxengine/law/qx_law_types.h"
#include "qxengine/intelligence/qx_snapshot.h"

#include <cstring>   // std::strcmp
#include <cstdarg>   // va_list, va_start, va_end
#include <cstdio>    // std::vsnprintf

extern "C" {
QX_API const char *qx_certification_label(QXCertificationId cert);
QX_API double qx_certification_min_score(QXCertificationId cert);
QX_API QXCertificationId qx_certification_from_score(double health_score);
}

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Static tables (internal linkage)
// ─────────────────────────────────────────────────────────────────────────────

namespace {

// ── Segment ID table ──────────────────────────────────────────────────────────

/**
 * @brief Canonical ordered table of the nine QLM segment identifiers.
 *
 * Index 0 = CORE (high-priority, 7 slots).
 * Index 8 = TEMP (low-priority,  6 slots).
 * Order is authoritative for all index-based lookups in the engine.
 */
static const char * const kSegmentIds[QX_SEGMENT_COUNT] = {
    QX_SEGMENT_CORE,  /* QX_SEGMENT_ID_CORE  – high-priority */
    QX_SEGMENT_UI,    /* QX_SEGMENT_ID_UI    – high-priority */
    QX_SEGMENT_DATA,  /* QX_SEGMENT_ID_DATA  – high-priority */
    QX_SEGMENT_IMG,   /* QX_SEGMENT_ID_IMG   – high-priority */
    QX_SEGMENT_NET,   /* QX_SEGMENT_ID_NET   – high-priority */
    QX_SEGMENT_AI,    /* QX_SEGMENT_ID_AI    – high-priority */
    QX_SEGMENT_SEC,   /* QX_SEGMENT_ID_SEC   – low-priority  */
    QX_SEGMENT_LOG,   /* QX_SEGMENT_ID_LOG   – low-priority  */
    QX_SEGMENT_TEMP   /* QX_SEGMENT_ID_TEMP  – low-priority  */
};

// ── Slot count table ──────────────────────────────────────────────────────────

/**
 * @brief Slot counts matching kSegmentIds by index.
 *
 * High-priority (0–5): QX_SLOTS_HIGH_PRIORITY (7) each → 42 slots.
 * Low-priority  (6–8): QX_SLOTS_LOW_PRIORITY  (6) each → 18 slots.
 * Total: QX_SLOT_COUNT (60).
 */
static const uint32_t kSegmentSlots[QX_SEGMENT_COUNT] = {
    QX_SLOTS_HIGH_PRIORITY,  /* CORE */
    QX_SLOTS_HIGH_PRIORITY,  /* UI   */
    QX_SLOTS_HIGH_PRIORITY,  /* DATA */
    QX_SLOTS_HIGH_PRIORITY,  /* IMG  */
    QX_SLOTS_HIGH_PRIORITY,  /* NET  */
    QX_SLOTS_HIGH_PRIORITY,  /* AI   */
    QX_SLOTS_LOW_PRIORITY,   /* SEC  */
    QX_SLOTS_LOW_PRIORITY,   /* LOG  */
    QX_SLOTS_LOW_PRIORITY    /* TEMP */
};

// ── Law name table ────────────────────────────────────────────────────────────

/**
 * @brief Human-readable law names indexed by (QXLawId - 1).
 *
 * QXLawId is 1-based; subtract 1 before indexing into this table.
 */
static const char * const kLawNames[QX_LAW_COUNT] = {
    "Pattern",      /* Law 1 */
    "Limit",        /* Law 2 */
    "Pairs",        /* Law 3 */
    "Equilibrium",  /* Law 4 */
    "Knowledge",    /* Law 5 */
    "Ethics",       /* Law 6 */
    "Creativity",   /* Law 7 */
    "Economy"       /* Law 8 */
};

// ── Severity name table ───────────────────────────────────────────────────────

/**
 * @brief Human-readable severity names indexed by QXSeverityId value.
 *
 * QXSeverityId is 0-based (INFO=0, FATAL=3).
 */
static const char * const kSeverityNames[4] = {
    "INFO",      /* 0 – QX_SEVERITY_INFO     */
    "WARNING",   /* 1 – QX_SEVERITY_WARNING  */
    "CRITICAL",  /* 2 – QX_SEVERITY_CRITICAL */
    "FATAL"      /* 3 – QX_SEVERITY_FATAL    */
};

// ── Certification tier table ──────────────────────────────────────────────────

struct CertEntry {
    QXCertificationId id;
    const char       *label;
    double            min_score;
};

static const CertEntry kCertTable[4] = {
    { QX_CERT_INVALID,      "Invalid",      0.0  },
    { QX_CERT_STANDARD,     "Standard",     60.0 },
    { QX_CERT_PROFESSIONAL, "Professional", 80.0 },
    { QX_CERT_SOVEREIGN,    "Sovereign",    95.0 }
};

// ── Thread-local error detail buffer ──────────────────────────────────────────

/**
 * @brief Thread-local diagnostic message buffer.
 *
 * Written by qx::internal::set_error_detail().
 * Read by the public C ABI function qx_error_detail().
 */
static thread_local char tl_error_detail[512] = { '\0' };

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Internal C++ helpers (non-exported)
// ─────────────────────────────────────────────────────────────────────────────

namespace qx::internal {
QXCertificationId certification_from_score(double score) noexcept;
QXCertificationId certification_from_score(double score) noexcept {
    if (score >= QX_CERT_SOVEREIGN_MIN)    { return QX_CERT_SOVEREIGN;    }
    if (score >= QX_CERT_PROFESSIONAL_MIN) { return QX_CERT_PROFESSIONAL; }
    if (score >= QX_CERT_STANDARD_MIN)     { return QX_CERT_STANDARD;     }
    return QX_CERT_INVALID;
}


int         segment_index(const char *segment_id) noexcept;
uint32_t    segment_slot_count(uint32_t index) noexcept;
const char *segment_id_at(uint32_t index) noexcept;
const char *law_name(QXLawId law_id) noexcept;
const char *severity_name(QXSeverityId severity) noexcept;

[[maybe_unused]] static void set_error_detail(const char *fmt, ...) noexcept {
    if (!fmt) { tl_error_detail[0] = '\0'; return; }
    va_list args;
    va_start(args, fmt);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
    std::vsnprintf(tl_error_detail, sizeof(tl_error_detail), fmt, args);
#pragma clang diagnostic pop
    va_end(args);
}

[[maybe_unused]] static void clear_error_detail() noexcept {
    tl_error_detail[0] = '\0';
}

int segment_index(const char *segment_id) noexcept {
    if (!segment_id) { return -1; }
    for (uint32_t i = 0u; i < QX_SEGMENT_COUNT; ++i) {
        if (std::strcmp(segment_id, kSegmentIds[i]) == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

uint32_t segment_slot_count(uint32_t index) noexcept {
    if (index >= QX_SEGMENT_COUNT) { return 0u; }
    return kSegmentSlots[index];
}

const char *segment_id_at(uint32_t index) noexcept {
    if (index >= QX_SEGMENT_COUNT) { return nullptr; }
    return kSegmentIds[index];
}

const char *law_name(QXLawId law_id) noexcept {
    if (law_id < 1 ||
        static_cast<uint32_t>(law_id) > QX_LAW_COUNT) {
        return "Unknown";
    }
    return kLawNames[static_cast<uint32_t>(law_id) - 1u];
}

const char *severity_name(QXSeverityId severity) noexcept {
    if (static_cast<uint32_t>(severity) >= 4u) { return "Unknown"; }
    return kSeverityNames[static_cast<uint32_t>(severity)];
}



static double certification_min_score(QXCertificationId cert) noexcept {
    for (uint32_t i = 0u; i < 4u; ++i) {
        if (kCertTable[i].id == cert) { return kCertTable[i].min_score; }
    }
    return 0.0;
}

static const char *certification_label(QXCertificationId cert) noexcept {
    for (uint32_t i = 0u; i < 4u; ++i) {
        if (kCertTable[i].id == cert) { return kCertTable[i].label; }
    }
    return "Unknown";
}

} // namespace qx::internal

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Public C ABI
// ─────────────────────────────────────────────────────────────────────────────

extern "C" {

QX_API const char *qx_error_detail(void) {
    return tl_error_detail;
}

QX_API const char *qx_certification_label(QXCertificationId cert) {
    return qx::internal::certification_label(cert);
}

QX_API double qx_certification_min_score(QXCertificationId cert) {
    return qx::internal::certification_min_score(cert);
}

QX_API QXCertificationId qx_certification_from_score(double health_score) {
    return qx::internal::certification_from_score(health_score);
}

} // extern "C"

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Compile-time invariant checks
// ─────────────────────────────────────────────────────────────────────────────

namespace {

static_assert(
    QX_SLOTS_HIGH_PRIORITY * 6u + QX_SLOTS_LOW_PRIORITY * 3u == QX_SLOT_COUNT,
    "Slot total must equal QX_SLOT_COUNT (60)"
);

static_assert(
    sizeof(kSegmentIds) / sizeof(kSegmentIds[0]) == QX_SEGMENT_COUNT,
    "kSegmentIds size must equal QX_SEGMENT_COUNT (9)"
);

static_assert(
    sizeof(kSegmentSlots) / sizeof(kSegmentSlots[0]) == QX_SEGMENT_COUNT,
    "kSegmentSlots size must equal QX_SEGMENT_COUNT (9)"
);

static_assert(
    sizeof(kLawNames) / sizeof(kLawNames[0]) == QX_LAW_COUNT,
    "kLawNames size must equal QX_LAW_COUNT (8)"
);

static_assert(
    sizeof(QX_LAW_WEIGHTS) / sizeof(QX_LAW_WEIGHTS[0]) == QX_LAW_COUNT,
    "QX_LAW_WEIGHTS size must equal QX_LAW_COUNT (8)"
);

static_assert(
    sizeof(QX_SIGNAL_WEIGHTS) / sizeof(QX_SIGNAL_WEIGHTS[0])
        == QX_SNAPSHOT_SIGNAL_COUNT,
    "QX_SIGNAL_WEIGHTS size must equal QX_SNAPSHOT_SIGNAL_COUNT (5)"
);

} // anonymous namespace
