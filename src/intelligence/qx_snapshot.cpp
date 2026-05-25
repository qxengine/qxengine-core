// =============================================================================
// QXEngine Core – src/intelligence/qx_snapshot.cpp
// Owner : Masa Bayu
// Created: 2026-05-24
// Repo   : https://github.com/qxengine/qxengine-core
// Purpose: Captures periodic cognitive snapshots, computes the five-signal
//          knowledge score, and maintains a ring-buffer history of 720 entries.
// =============================================================================

#include "qxengine/intelligence/qx_snapshot.h"
#include "qxengine/core/qx_types.h"
#include "qxengine/core/qx_constants.h"
#include "qxengine/core/qx_error.h"

#include <cstring>
#include <cmath>
#include <ctime>
#include <mutex>
#include <atomic>
#include <new>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace {

static QXTimestamp now_ms() noexcept {
    struct timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);
    return static_cast<QXTimestamp>(
        static_cast<int64_t>(ts.tv_sec) * 1000LL + ts.tv_nsec / 1'000'000LL);
}

static double clamp_d(double v, double lo, double hi) noexcept {
    return v < lo ? lo : (v > hi ? hi : v);
}

static double signal_clarity(double utilisation) noexcept {
    return clamp_d(1.0 - std::fabs(utilisation - 0.45) / 0.55, 0.0, 1.0);
}

static double composite_clarity(const double *utilisations, uint32_t count) noexcept {
    if (count == 0u) { return 0.0; }
    double sum = 0.0;
    for (uint32_t i = 0u; i < count; ++i) {
        sum += signal_clarity(utilisations[i]);
    }
    return sum / static_cast<double>(count);
}

static double signal_stability(double pressure_variance) noexcept {
    return clamp_d(1.0 - pressure_variance / 3.0, 0.0, 1.0);
}

static double signal_compliance(double health_score, uint32_t streak) noexcept {
    const double base  = clamp_d(health_score / 100.0, 0.0, 1.0);
    const uint32_t cap = streak < 12u ? streak : 12u;
    const double bonus = clamp_d(static_cast<double>(cap) * (0.05 / 12.0), 0.0, 0.05);
    return clamp_d(base + bonus, 0.0, 1.0);
}

static double signal_native(double coverage_ratio) noexcept {
    return clamp_d(coverage_ratio, 0.0, 1.0);
}

static double signal_recency(double seconds_elapsed) noexcept {
    return clamp_d(1.0 - seconds_elapsed / QX_SNAPSHOT_RECENCY_DECAY_SEC, 0.0, 1.0);
}

static void build_snapshot(const QXSnapshotInput *in,
                           uint64_t               sequence,
                           double                 seconds_since_previous,
                           double                 pressure_tier_variance,
                           QXCognitiveSnapshot   *out) noexcept {
    std::memset(out, 0, sizeof(*out));
    out->sequence        = sequence;
    out->captured_at_ms  = in->captured_at_ms != 0u ? in->captured_at_ms : now_ms();

    const uint32_t seg_count = in->segment_count > QX_SEGMENT_COUNT
                             ? QX_SEGMENT_COUNT : in->segment_count;

    out->signal_values[QX_SIGNAL_MEMORY_CLARITY] =
        composite_clarity(in->segment_utilisations, seg_count);
    out->signal_values[QX_SIGNAL_PRESSURE_STABILITY] =
        signal_stability(pressure_tier_variance);
    out->signal_values[QX_SIGNAL_LAW_COMPLIANCE] =
        signal_compliance(in->latest_law_health_score, in->compliance_streak);
    out->signal_values[QX_SIGNAL_NATIVE_COVERAGE] =
        signal_native(in->native_coverage_ratio);
    out->signal_values[QX_SIGNAL_RECENCY] =
        signal_recency(seconds_since_previous);

    double score = 0.0;
    for (uint32_t i = 0u; i < QX_SNAPSHOT_SIGNAL_COUNT; ++i) {
        out->weighted_contributions[i] =
            out->signal_values[i] * QX_SIGNAL_WEIGHTS[i];
        score += out->weighted_contributions[i];
    }
    out->knowledge_score = clamp_d(score * 100.0, 0.0, 100.0);
    out->is_law5_compliant = (out->knowledge_score >= QX_SNAPSHOT_MIN_SCORE)
                           ? QX_TRUE : QX_FALSE;

    out->segment_clarity_count = seg_count;
    for (uint32_t i = 0u; i < seg_count; ++i) {
        std::strncpy(out->segment_clarity[i].segment_id,
                     in->segment_ids[i], 15u);
        out->segment_clarity[i].segment_id[15] = '\0';
        out->segment_clarity[i].hard_utilisation = in->segment_utilisations[i];
        out->segment_clarity[i].clarity_score =
            signal_clarity(in->segment_utilisations[i]);
        out->segment_clarity[i].pressure_tier = in->composite_pressure_tier;
    }

    out->composite_memory_clarity = out->signal_values[QX_SIGNAL_MEMORY_CLARITY];
    out->total_used_bytes         = in->total_used_bytes;
    out->composite_pressure_tier  = in->composite_pressure_tier;
    out->pressure_tier_variance   = pressure_tier_variance;
    out->latest_law_health_score  = in->latest_law_health_score;
    out->compliance_streak        = in->compliance_streak;
    out->native_coverage_ratio    = in->native_coverage_ratio;
    out->declared_capability_count = in->declared_capability_count;
    out->active_capability_count  = in->active_capability_count;
    out->seconds_since_previous   = seconds_since_previous;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// QXSnapshotHistory_s struct
// ---------------------------------------------------------------------------
struct QXSnapshotHistory_s {
    QXCognitiveSnapshot history[QX_SNAPSHOT_HISTORY_MAX];
    uint32_t            count;
    uint32_t            head;

    double              peak_score;
    double              trough_score;
    double              score_sum;
    uint32_t            non_compliant_count;
    QXTimestamp         last_captured_at_ms;
    uint64_t            total_captured;

    std::atomic<uint64_t> sequence{0};
    mutable std::mutex    mtx;
};

namespace {

static uint32_t snap_physical(const QXSnapshotHistory_s *h, uint32_t logical) noexcept {
    if (h->count < QX_SNAPSHOT_HISTORY_MAX) { return logical; }
    return (h->head + logical) % QX_SNAPSHOT_HISTORY_MAX;
}

static void snap_append(QXSnapshotHistory_s *h, const QXCognitiveSnapshot &s) noexcept {
    h->history[h->head] = s;
    h->head = (h->head + 1u) % QX_SNAPSHOT_HISTORY_MAX;
    if (h->count < QX_SNAPSHOT_HISTORY_MAX) { ++h->count; }

    h->score_sum += s.knowledge_score;
    if (s.knowledge_score > h->peak_score) { h->peak_score = s.knowledge_score; }
    if (s.knowledge_score < h->trough_score || h->total_captured == 0u) {
        h->trough_score = s.knowledge_score;
    }
    if (!s.is_law5_compliant) { ++h->non_compliant_count; }
    h->last_captured_at_ms = s.captured_at_ms;
    ++h->total_captured;
}

} // anonymous namespace

static QXSnapshotHistory_s *as_history(QXSnapshotHistoryHandle handle) noexcept {
    return static_cast<QXSnapshotHistory_s *>(handle);
}

// ---------------------------------------------------------------------------
// Public C ABI
// ---------------------------------------------------------------------------
extern "C" {

QX_API QXResult qx_snapshot_history_create(QXSnapshotHistoryHandle *out_handle) {
    if (!out_handle) { return QX_ERROR_INVALID_ARGUMENT; }
    auto *h = new (std::nothrow) QXSnapshotHistory_s{};
    if (!h) { return QX_ERROR_OUT_OF_MEMORY; }
    h->trough_score = 100.0;
    *out_handle = h;
    return QX_OK;
}

QX_API void qx_snapshot_history_destroy(QXSnapshotHistoryHandle handle) {
    delete as_history(handle);
}

QX_API QXResult qx_snapshot_capture(QXSnapshotHistoryHandle  handle,
                                     const QXSnapshotInput   *input,
                                     QXCognitiveSnapshot     *out_snapshot) {
    if (!handle || !input) { return QX_ERROR_INVALID_ARGUMENT; }
    QXSnapshotHistory_s *h = as_history(handle);

    std::lock_guard<std::mutex> lk(h->mtx);
    const uint64_t seq = ++h->sequence;

    double seconds_since_previous = 0.0;
    if (h->last_captured_at_ms > 0u &&
        input->captured_at_ms > h->last_captured_at_ms) {
        seconds_since_previous =
            static_cast<double>(input->captured_at_ms - h->last_captured_at_ms)
            / 1000.0;
    }

    QXCognitiveSnapshot snap{};
    build_snapshot(input, seq, seconds_since_previous, 0.0, &snap);
    snap_append(h, snap);

    if (out_snapshot) { *out_snapshot = snap; }
    return QX_OK;
}

QX_API QXResult qx_snapshot_count(QXSnapshotHistoryHandle handle,
                                   uint32_t               *out_count) {
    if (!handle || !out_count) { return QX_ERROR_INVALID_ARGUMENT; }
    QXSnapshotHistory_s *h = as_history(handle);
    std::lock_guard<std::mutex> lk(h->mtx);
    *out_count = h->count;
    return QX_OK;
}

QX_API QXResult qx_snapshot_at(QXSnapshotHistoryHandle  handle,
                                uint32_t                 index,
                                QXCognitiveSnapshot     *out_snapshot) {
    if (!handle || !out_snapshot) { return QX_ERROR_INVALID_ARGUMENT; }
    QXSnapshotHistory_s *h = as_history(handle);
    std::lock_guard<std::mutex> lk(h->mtx);
    if (index >= h->count) { return QX_ERROR_OUT_OF_RANGE; }
    *out_snapshot = h->history[snap_physical(h, index)];
    return QX_OK;
}

QX_API QXResult qx_snapshot_latest(QXSnapshotHistoryHandle  handle,
                                    QXCognitiveSnapshot     *out_snapshot) {
    if (!handle || !out_snapshot) { return QX_ERROR_INVALID_ARGUMENT; }
    QXSnapshotHistory_s *h = as_history(handle);
    std::lock_guard<std::mutex> lk(h->mtx);
    if (h->count == 0u) { return QX_ERROR_NOT_FOUND; }
    const uint32_t last = (h->count < QX_SNAPSHOT_HISTORY_MAX)
                        ? h->count - 1u
                        : (h->head + QX_SNAPSHOT_HISTORY_MAX - 1u)
                          % QX_SNAPSHOT_HISTORY_MAX;
    *out_snapshot = h->history[last];
    return QX_OK;
}

QX_API QXResult qx_snapshot_copy_all(QXSnapshotHistoryHandle  handle,
                                      QXCognitiveSnapshot     *out_buf,
                                      uint32_t                 capacity,
                                      uint32_t                *out_written) {
    if (!handle || !out_buf || capacity == 0u) { return QX_ERROR_INVALID_ARGUMENT; }
    QXSnapshotHistory_s *h = as_history(handle);
    std::lock_guard<std::mutex> lk(h->mtx);
    const uint32_t n = h->count < capacity ? h->count : capacity;
    for (uint32_t i = 0u; i < n; ++i) {
        out_buf[i] = h->history[snap_physical(h, i)];
    }
    if (out_written) { *out_written = n; }
    return QX_OK;
}

QX_API QXResult qx_snapshot_find_non_compliant(QXSnapshotHistoryHandle  handle,
                                                QXCognitiveSnapshot     *out_snapshot) {
    if (!handle || !out_snapshot) { return QX_ERROR_INVALID_ARGUMENT; }
    QXSnapshotHistory_s *h = as_history(handle);
    std::lock_guard<std::mutex> lk(h->mtx);
    for (uint32_t i = 0u; i < h->count; ++i) {
        const QXCognitiveSnapshot &s = h->history[snap_physical(h, i)];
        if (!s.is_law5_compliant) { *out_snapshot = s; return QX_OK; }
    }
    return QX_ERROR_NOT_FOUND;
}

QX_API QXResult qx_snapshot_clear(QXSnapshotHistoryHandle handle) {
    if (!handle) { return QX_ERROR_INVALID_ARGUMENT; }
    QXSnapshotHistory_s *h = as_history(handle);
    std::lock_guard<std::mutex> lk(h->mtx);
    h->count               = 0u;
    h->head                = 0u;
    h->score_sum           = 0.0;
    h->peak_score          = 0.0;
    h->trough_score        = 100.0;
    h->non_compliant_count = 0u;
    return QX_OK;
}

QX_API QXResult qx_snapshot_history_stats(QXSnapshotHistoryHandle  handle,
                                           QXSnapshotHistoryStats  *out_stats) {
    if (!handle || !out_stats) { return QX_ERROR_INVALID_ARGUMENT; }
    QXSnapshotHistory_s *h = as_history(handle);
    std::lock_guard<std::mutex> lk(h->mtx);
    std::memset(out_stats, 0, sizeof(*out_stats));
    out_stats->snapshot_count          = h->count;
    out_stats->total_captured          = h->total_captured;
    out_stats->non_compliant_count     = h->non_compliant_count;
    out_stats->peak_knowledge_score    = h->peak_score;
    out_stats->trough_knowledge_score  = h->trough_score;
    out_stats->mean_knowledge_score = h->count > 0u
        ? h->score_sum / static_cast<double>(h->count) : 0.0;
    if (h->count > 0u) {
        const uint32_t last = (h->count < QX_SNAPSHOT_HISTORY_MAX)
                            ? h->count - 1u
                            : (h->head + QX_SNAPSHOT_HISTORY_MAX - 1u)
                              % QX_SNAPSHOT_HISTORY_MAX;
        const QXCognitiveSnapshot &latest = h->history[last];
        out_stats->last_knowledge_score = latest.knowledge_score;
        out_stats->latest_snapshot_ms   = latest.captured_at_ms;
        out_stats->oldest_snapshot_ms   =
            h->history[snap_physical(h, 0u)].captured_at_ms;
        if (h->last_captured_at_ms > 0u) {
            out_stats->seconds_since_last_capture =
                static_cast<double>(now_ms() - h->last_captured_at_ms) / 1000.0;
        }
    }
    return QX_OK;
}

QX_API double qx_snapshot_clarity_for_utilisation(double hard_utilisation) {
    return signal_clarity(hard_utilisation);
}

QX_API double qx_snapshot_recency_signal(double seconds_elapsed) {
    return signal_recency(seconds_elapsed);
}

QX_API double qx_snapshot_compliance_signal(double    health_score,
                                            uint32_t  compliance_streak) {
    return signal_compliance(health_score, compliance_streak);
}

} // extern "C"

static_assert(QX_SNAPSHOT_HISTORY_MAX  == 720u, "history cap mismatch");
static_assert(QX_SNAPSHOT_SIGNAL_COUNT == 5u,   "signal count mismatch");
static_assert(QX_SNAPSHOT_RECENCY_DECAY_SEC > 0.0, "decay sec must be positive");
