// =============================================================================
// QXEngine Core – src/telemetry/qx_telemetry.cpp
// Owner : Masa Bayu
// Created: 2026-05-24
// Repo   : https://github.com/qxengine/qxengine-core
// =============================================================================

#include "qxengine/telemetry/qx_telemetry.h"
#include "qxengine/telemetry/qx_telemetry_types.h"
#include "qxengine/core/qx_types.h"
#include "qxengine/core/qx_constants.h"
#include "qxengine/core/qx_error.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <atomic>
#include <time.h>

namespace {

static int64_t now_ms() noexcept {
    struct timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1000LL
         + static_cast<int64_t>(ts.tv_nsec) / 1'000'000LL;
}

static const char *category_label(QXTelemetryCategory c) noexcept {
    if      (c == QX_TEL_ALLOCATION)   return "Allocation";
    else if (c == QX_TEL_DEALLOCATION) return "Deallocation";
    else if (c == QX_TEL_EVICTION)     return "Eviction";
    else if (c == QX_TEL_PRESSURE)     return "Pressure";
    else if (c == QX_TEL_EVALUATION)   return "Evaluation";
    else if (c == QX_TEL_SNAPSHOT)     return "Snapshot";
    else if (c == QX_TEL_ENGINE)       return "Engine";
    else if (c == QX_TEL_VIOLATION)    return "Violation";
    else if (c == QX_TEL_SECURITY)     return "Security";
    else if (c == QX_TEL_NATIVE)       return "Native";
    return "Unknown";
}

} // anonymous namespace

struct QXTelemetry_s {
    QXTelemetryConfig config;

    QXTelemetryEvent events[QX_TEL_EVENT_CAP];
    uint32_t         event_count{0};
    uint32_t         event_head{0};

    QXViolation      violations[QX_TEL_VIOLATION_CAP];
    uint32_t         violation_count{0};
    uint32_t         violation_head{0};

    uint64_t total_events{0};
    uint64_t total_violations{0};
    uint64_t category_counts[10]{};
    uint64_t law_violation_counts[QX_LAW_COUNT]{};
    uint64_t total_bytes_allocated{0};
    uint64_t total_bytes_deallocated{0};
    uint64_t total_bytes_evicted{0};
    int64_t  first_event_ms{0};
    int64_t  last_event_ms{0};

    std::atomic<uint64_t> sequence{0};
    mutable std::mutex    mtx;
};

// ---------------------------------------------------------------------------
// Ring-buffer helpers
// ---------------------------------------------------------------------------
namespace {

static void event_append(QXTelemetry_s *t, const QXTelemetryEvent &ev) noexcept {
    t->events[t->event_head] = ev;
    t->event_head = (t->event_head + 1u) % QX_TEL_EVENT_CAP;
    if (t->event_count < QX_TEL_EVENT_CAP) ++t->event_count;
}

static void violation_append(QXTelemetry_s *t, const QXViolation &v) noexcept {
    t->violations[t->violation_head] = v;
    t->violation_head = (t->violation_head + 1u) % QX_TEL_VIOLATION_CAP;
    if (t->violation_count < QX_TEL_VIOLATION_CAP) ++t->violation_count;
}

static uint32_t event_physical(const QXTelemetry_s *t, uint32_t logical) noexcept {
    if (t->event_count < QX_TEL_EVENT_CAP) return logical;
    return (t->event_head + logical) % QX_TEL_EVENT_CAP;
}

static uint32_t violation_physical(const QXTelemetry_s *t,
                                    uint32_t logical) noexcept {
    if (t->violation_count < QX_TEL_VIOLATION_CAP) return logical;
    return (t->violation_head + logical) % QX_TEL_VIOLATION_CAP;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public C ABI
// ---------------------------------------------------------------------------
extern "C" {

// ---- Lifecycle ------------------------------------------------------------

QX_API QXResult qx_telemetry_create(const QXTelemetryConfig *config,
                                     QXTelemetryHandle       *out_telemetry) {
    if (!out_telemetry) return QX_ERR_NULL_ARG;

    auto *t = new (std::nothrow) QXTelemetry_s{};
    if (!t) return QX_ERR_INTERNAL;

    if (config) {
        t->config = *config;
    } else {
        t->config.enabled                = QX_TRUE;
        t->config.record_allocations     = QX_TRUE;
        t->config.record_evictions       = QX_TRUE;
        t->config.record_evaluations     = QX_TRUE;
        t->config.record_snapshots       = QX_TRUE;
        t->config.record_security        = QX_TRUE;
        t->config.separate_violation_log = QX_TRUE;
        t->config.verbose                = QX_FALSE;
    }

    *out_telemetry = t;
    return QX_OK;
}

QX_API void qx_telemetry_destroy(QXTelemetryHandle telemetry) {
    delete telemetry;
}

// ---- Recording ------------------------------------------------------------

QX_API QXResult qx_telemetry_record(QXTelemetryHandle            telemetry,
                                     const QXTelemetryEventInput *input,
                                     QXTelemetryEvent            *out_event) {
    if (!telemetry || !input) return QX_ERR_NULL_ARG;
    if (!telemetry->config.enabled) return QX_OK;

    std::lock_guard<std::mutex> lk(telemetry->mtx);

    QXTelemetryEvent ev{};
    ev.sequence      = ++telemetry->sequence;
    ev.category      = input->category;
    ev.timestamp_ms  = static_cast<QXTimestamp>(now_ms());
    ev.result        = input->result;
    ev.is_success    = (input->result == QX_OK) ? QX_TRUE : QX_FALSE;
    ev.bytes         = input->bytes;
    ev.health_score  = input->health_score;
    ev.pressure_tier = input->pressure_tier;
    ev.law_id        = input->law_id;
    ev.severity      = input->severity;
    ev.tag_count     = input->tag_count < QX_TEL_MAX_TAGS
                       ? input->tag_count : QX_TEL_MAX_TAGS;

    std::strncpy(ev.message,    input->message,    QX_TEL_MESSAGE_MAX - 1);
    std::strncpy(ev.detail,     input->detail,     QX_TEL_DETAIL_MAX  - 1);
    std::strncpy(ev.leaf_id,    input->leaf_id,    sizeof(ev.leaf_id)  - 1);
    std::strncpy(ev.leaf_label, input->leaf_label, sizeof(ev.leaf_label) - 1);
    std::strncpy(ev.segment_id, input->segment_id, sizeof(ev.segment_id) - 1);

    for (uint32_t i = 0; i < ev.tag_count; ++i)
        ev.tags[i] = input->tags[i];

    event_append(telemetry, ev);
    ++telemetry->total_events;
    ++telemetry->category_counts[static_cast<int>(ev.category)];

    if (telemetry->first_event_ms == 0)
        telemetry->first_event_ms = static_cast<int64_t>(ev.timestamp_ms);
    telemetry->last_event_ms = static_cast<int64_t>(ev.timestamp_ms);

    if (ev.category == QX_TEL_ALLOCATION)
        telemetry->total_bytes_allocated   += ev.bytes;
    else if (ev.category == QX_TEL_DEALLOCATION)
        telemetry->total_bytes_deallocated += ev.bytes;
    else if (ev.category == QX_TEL_EVICTION)
        telemetry->total_bytes_evicted     += ev.bytes;

    if (out_event) *out_event = ev;
    return QX_OK;
}

QX_API QXResult qx_telemetry_record_violation(QXTelemetryHandle    telemetry,
                                               const QXViolation   *violation,
                                               QXTelemetryEvent    *out_event) {
    if (!telemetry || !violation) return QX_ERR_NULL_ARG;
    if (!telemetry->config.enabled) return QX_OK;

    std::lock_guard<std::mutex> lk(telemetry->mtx);

    QXTelemetryEvent ev{};
    ev.sequence     = ++telemetry->sequence;
    ev.category     = QX_TEL_VIOLATION;
    ev.timestamp_ms = static_cast<QXTimestamp>(now_ms());
    ev.result       = QX_ERR_NULL_ARG; // placeholder – no law-violation error code available here
    ev.is_success   = QX_FALSE;
    ev.law_id       = violation->law_id;
    ev.severity     = violation->severity;

    std::strncpy(ev.message, violation->message, QX_TEL_MESSAGE_MAX - 1);
    std::strncpy(ev.detail,  violation->detail,  QX_TEL_DETAIL_MAX  - 1);

    if (telemetry->config.separate_violation_log)
        violation_append(telemetry, *violation);

    event_append(telemetry, ev);
    ++telemetry->total_events;
    ++telemetry->total_violations;
    ++telemetry->category_counts[static_cast<int>(QX_TEL_VIOLATION)];

    if (violation->law_id < QX_LAW_COUNT)
        ++telemetry->law_violation_counts[violation->law_id];

    if (telemetry->first_event_ms == 0)
        telemetry->first_event_ms = static_cast<int64_t>(ev.timestamp_ms);
    telemetry->last_event_ms = static_cast<int64_t>(ev.timestamp_ms);

    if (out_event) *out_event = ev;
    return QX_OK;
}

QX_API QXResult qx_telemetry_record_report(QXTelemetryHandle   telemetry,
                                            const QXLawReport  *report,
                                            uint32_t           *out_recorded) {
    if (!telemetry || !report) return QX_ERR_NULL_ARG;
    if (!telemetry->config.enabled) return QX_OK;

    uint32_t recorded = 0;

    // Record each violation individually
    for (uint32_t i = 0; i < report->violation_count; ++i) {
        QXResult r = qx_telemetry_record_violation(telemetry,
                                                    &report->violations[i],
                                                    nullptr);
        if (r == QX_OK) ++recorded;
    }

    // Record one summary EVALUATION event
    {
        std::lock_guard<std::mutex> lk(telemetry->mtx);

        QXTelemetryEvent ev{};
        ev.sequence     = ++telemetry->sequence;
        ev.category     = QX_TEL_EVALUATION;
        ev.timestamp_ms = static_cast<QXTimestamp>(now_ms());
        ev.result       = QX_OK;
        ev.is_success   = report->is_fully_compliant;
        ev.health_score = report->health_score;

        std::snprintf(ev.message, QX_TEL_MESSAGE_MAX,
            "LawReport seq=%llu score=%.1f cert=%d violations=%u",
            static_cast<unsigned long long>(report->sequence),
            static_cast<double>(report->health_score),
            static_cast<int>(report->certification),
            report->violation_count);

        event_append(telemetry, ev);
        ++telemetry->total_events;
        ++telemetry->category_counts[static_cast<int>(QX_TEL_EVALUATION)];

        if (telemetry->first_event_ms == 0)
            telemetry->first_event_ms = static_cast<int64_t>(ev.timestamp_ms);
        telemetry->last_event_ms = static_cast<int64_t>(ev.timestamp_ms);
        ++recorded;
    }

    if (out_recorded) *out_recorded = recorded;
    return QX_OK;
}

// ---- Event log access -----------------------------------------------------

QX_API QXResult qx_telemetry_event_count(QXTelemetryHandle telemetry,
                                          uint32_t         *out_count) {
    if (!telemetry || !out_count) return QX_ERR_NULL_ARG;
    std::lock_guard<std::mutex> lk(telemetry->mtx);
    *out_count = telemetry->event_count;
    return QX_OK;
}

QX_API QXResult qx_telemetry_event_at(QXTelemetryHandle  telemetry,
                                       uint32_t           index,
                                       QXTelemetryEvent  *out_event) {
    if (!telemetry || !out_event) return QX_ERR_NULL_ARG;
    std::lock_guard<std::mutex> lk(telemetry->mtx);
    if (index >= telemetry->event_count) return QX_ERR_NULL_ARG;
    *out_event = telemetry->events[event_physical(telemetry, index)];
    return QX_OK;
}

QX_API QXResult qx_telemetry_events_copy(QXTelemetryHandle  telemetry,
                                          QXTelemetryEvent  *out_events,
                                          uint32_t           capacity,
                                          uint32_t          *out_written) {
    if (!telemetry || !out_events || capacity == 0) return QX_ERR_NULL_ARG;
    std::lock_guard<std::mutex> lk(telemetry->mtx);
    uint32_t n = telemetry->event_count < capacity
                 ? telemetry->event_count : capacity;
    for (uint32_t i = 0; i < n; ++i)
        out_events[i] = telemetry->events[event_physical(telemetry, i)];
    if (out_written) *out_written = n;
    return QX_OK;
}

QX_API QXResult qx_telemetry_events_by_category(QXTelemetryHandle    telemetry,
                                                  QXTelemetryCategory  cat,
                                                  QXTelemetryEvent    *out_events,
                                                  uint32_t             capacity,
                                                  uint32_t            *out_written) {
    if (!telemetry || !out_events || capacity == 0) return QX_ERR_NULL_ARG;
    std::lock_guard<std::mutex> lk(telemetry->mtx);
    uint32_t written = 0;
    for (uint32_t i = 0; i < telemetry->event_count && written < capacity; ++i) {
        const QXTelemetryEvent &ev =
            telemetry->events[event_physical(telemetry, i)];
        if (ev.category == cat) out_events[written++] = ev;
    }
    if (out_written) *out_written = written;
    return QX_OK;
}

QX_API QXResult qx_telemetry_clear_events(QXTelemetryHandle telemetry) {
    if (!telemetry) return QX_ERR_NULL_ARG;
    std::lock_guard<std::mutex> lk(telemetry->mtx);
    telemetry->event_count = 0;
    telemetry->event_head  = 0;
    return QX_OK;
}

// ---- Violation log access -------------------------------------------------

QX_API QXResult qx_telemetry_violation_count(QXTelemetryHandle telemetry,
                                              uint32_t         *out_count) {
    if (!telemetry || !out_count) return QX_ERR_NULL_ARG;
    std::lock_guard<std::mutex> lk(telemetry->mtx);
    *out_count = telemetry->violation_count;
    return QX_OK;
}

QX_API QXResult qx_telemetry_violation_at(QXTelemetryHandle  telemetry,
                                           uint32_t           index,
                                           QXViolation       *out_violation) {
    if (!telemetry || !out_violation) return QX_ERR_NULL_ARG;
    std::lock_guard<std::mutex> lk(telemetry->mtx);
    if (index >= telemetry->violation_count) return QX_ERR_NULL_ARG;
    *out_violation = telemetry->violations[violation_physical(telemetry, index)];
    return QX_OK;
}

QX_API QXResult qx_telemetry_violations_copy(QXTelemetryHandle  telemetry,
                                              QXViolation       *out_violations,
                                              uint32_t           capacity,
                                              uint32_t          *out_written) {
    if (!telemetry || !out_violations || capacity == 0) return QX_ERR_NULL_ARG;
    std::lock_guard<std::mutex> lk(telemetry->mtx);
    uint32_t n = telemetry->violation_count < capacity
                 ? telemetry->violation_count : capacity;
    for (uint32_t i = 0; i < n; ++i)
        out_violations[i] =
            telemetry->violations[violation_physical(telemetry, i)];
    if (out_written) *out_written = n;
    return QX_OK;
}

QX_API QXResult qx_telemetry_violations_by_law(QXTelemetryHandle  telemetry,
                                                QXLawId            law_id,
                                                QXViolation       *out_violations,
                                                uint32_t           capacity,
                                                uint32_t          *out_written) {
    if (!telemetry || !out_violations || capacity == 0) return QX_ERR_NULL_ARG;
    if (law_id >= QX_LAW_COUNT) return QX_ERR_NULL_ARG;
    std::lock_guard<std::mutex> lk(telemetry->mtx);
    uint32_t written = 0;
    for (uint32_t i = 0;
         i < telemetry->violation_count && written < capacity; ++i) {
        const QXViolation &v =
            telemetry->violations[violation_physical(telemetry, i)];
        if (v.law_id == law_id) out_violations[written++] = v;
    }
    if (out_written) *out_written = written;
    return QX_OK;
}

QX_API QXResult qx_telemetry_clear_violations(QXTelemetryHandle telemetry) {
    if (!telemetry) return QX_ERR_NULL_ARG;
    std::lock_guard<std::mutex> lk(telemetry->mtx);
    telemetry->violation_count = 0;
    telemetry->violation_head  = 0;
    return QX_OK;
}

// ---- Reporting ------------------------------------------------------------

QX_API QXResult qx_telemetry_report(QXTelemetryHandle   telemetry,
                                     double              health_score,
                                     QXTelemetryReport  *out_report) {
    if (!telemetry || !out_report) return QX_ERR_NULL_ARG;
    if (health_score < 0.0 || health_score > 100.0) return QX_ERR_NULL_ARG;

    std::lock_guard<std::mutex> lk(telemetry->mtx);

    std::memset(out_report, 0, sizeof(*out_report));
    out_report->generated_at_ms  = static_cast<QXTimestamp>(now_ms());
    out_report->total_events      = telemetry->total_events;
    out_report->total_violations  = telemetry->total_violations;
    out_report->period_start_ms   = static_cast<QXTimestamp>(telemetry->first_event_ms);
    out_report->period_end_ms     = static_cast<QXTimestamp>(telemetry->last_event_ms);
    out_report->health_score      = health_score;

    for (int i = 0; i < 10; ++i)
        out_report->events_by_category[i] = telemetry->category_counts[i];

    std::snprintf(out_report->summary, QX_TEL_REPORT_SUMMARY_MAX,
        "QXEngine Telemetry | events=%llu violations=%llu score=%.1f",
        static_cast<unsigned long long>(telemetry->total_events),
        static_cast<unsigned long long>(telemetry->total_violations),
        health_score);

    return QX_OK;
}

QX_API QXResult qx_telemetry_report_to_json(const QXTelemetryReport *report,
                                             char                    *buffer,
                                             uint32_t                 buffer_size,
                                             uint32_t                *out_written) {
    if (!report || !buffer || buffer_size == 0) return QX_ERR_NULL_ARG;

    int n = std::snprintf(buffer, static_cast<size_t>(buffer_size),
        "{"
        "\"total_events\":%llu,"
        "\"total_violations\":%llu,"
        "\"health_score\":%.1f,"
        "\"period_start_ms\":%llu,"
        "\"period_end_ms\":%llu"
        "}",
        static_cast<unsigned long long>(report->total_events),
        static_cast<unsigned long long>(report->total_violations),
        report->health_score,
        static_cast<unsigned long long>(report->period_start_ms),
        static_cast<unsigned long long>(report->period_end_ms));

    if (n < 0) return QX_ERR_INTERNAL;
    if (out_written) *out_written = static_cast<uint32_t>(n);
    return (static_cast<uint32_t>(n) >= buffer_size)
           ? QX_ERR_BUFFER_TOO_SMALL : QX_OK;
}

// ---- Statistics -----------------------------------------------------------

QX_API QXResult qx_telemetry_stats(QXTelemetryHandle  telemetry,
                                    QXTelemetryStats  *out_stats) {
    if (!telemetry || !out_stats) return QX_ERR_NULL_ARG;
    std::lock_guard<std::mutex> lk(telemetry->mtx);

    std::memset(out_stats, 0, sizeof(*out_stats));
    out_stats->total_events_recorded     = telemetry->total_events;
    out_stats->current_event_count       = telemetry->event_count;
    out_stats->current_violation_count   = telemetry->violation_count;
    out_stats->total_violations_recorded = telemetry->total_violations;
    out_stats->total_fatal_violations    = 0u;
    out_stats->total_critical_violations = 0u;
    out_stats->total_allocation_events   = telemetry->category_counts[QX_TEL_ALLOCATION];
    out_stats->total_deallocation_events = telemetry->category_counts[QX_TEL_DEALLOCATION];
    out_stats->total_eviction_events     = telemetry->category_counts[QX_TEL_EVICTION];
    out_stats->total_pressure_events     = telemetry->category_counts[QX_TEL_PRESSURE];
    out_stats->total_evaluation_events   = telemetry->category_counts[QX_TEL_EVALUATION];
    out_stats->total_snapshot_events     = telemetry->category_counts[QX_TEL_SNAPSHOT];
    out_stats->first_event_ms            = static_cast<QXTimestamp>(telemetry->first_event_ms);
    out_stats->last_event_ms             = static_cast<QXTimestamp>(telemetry->last_event_ms);

    for (uint32_t i = 0; i < QX_LAW_COUNT; ++i)
        out_stats->law_violation_counts[i] = telemetry->law_violation_counts[i];

    return QX_OK;
}

// ---- Helper ---------------------------------------------------------------

QX_API const char *qx_telemetry_category_label(QXTelemetryCategory category) {
    return category_label(category);
}

} // extern "C"

// ---------------------------------------------------------------------------
// Compile-time assertions
// ---------------------------------------------------------------------------
static_assert(QX_TEL_EVENT_CAP     == 1000, "event cap mismatch");
static_assert(QX_TEL_VIOLATION_CAP == 1000, "violation cap mismatch");
static_assert(QX_TEL_MAX_TAGS      == 8,    "tag cap mismatch");
static_assert(QX_LAW_COUNT         == 8,    "law count mismatch");
