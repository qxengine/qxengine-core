// =============================================================================
// QXEngine Core – src/memory/qx_segment_api.cpp
// Owner : Masa Bayu
// Created: 2026-05-24
// Repo   : https://github.com/qxengine/qxengine-core
// Description: Public C ABI for QXSegment – all extern "C" functions exposed
//              to platform adapters and the QXEngine facade.
// =============================================================================

#include "qx_segment_internal.h"

#include <cstring>
#include <mutex>

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Internal helpers (local to this TU)
// ─────────────────────────────────────────────────────────────────────────────

namespace {

static QXTierId api_compute_tier(QXSize used_bytes,
                                  QXSize hard_limit_bytes) noexcept {
    if (hard_limit_bytes == 0u) { return QX_TIER_NORMAL; }
    const double u = static_cast<double>(used_bytes)
                   / static_cast<double>(hard_limit_bytes);
    if (u >= QX_TIER_4_THRESHOLD_PCT / 100.0) { return QX_TIER_CRITICAL;  }
    if (u >= QX_TIER_3_THRESHOLD_PCT / 100.0) { return QX_TIER_HIGH;      }
    if (u >= QX_TIER_2_THRESHOLD_PCT / 100.0) { return QX_TIER_ELEVATED;  }
    return QX_TIER_NORMAL;
}

static double api_pairs_ratio(uint64_t allocs,
                               uint64_t deallocs) noexcept {
    if (allocs == 0u) { return 1.0; }
    const double r = static_cast<double>(deallocs)
                   / static_cast<double>(allocs);
    return (r > 1.0) ? 1.0 : r;
}

static QXSize api_orphaned_bytes(QXSegment_s *seg) noexcept {
    QXSize orphaned = 0u;
    const QXTimestamp now     = qx_seg_now_ms();
    const QXTimestamp idle_ms =
        static_cast<QXTimestamp>(QX_ORPHAN_IDLE_SEC) * 1000u;

    for (uint32_t i = 0u; i < seg->max_slots; ++i) {
        QXLeafHandle lf = seg->slots[i];
        if (!lf) { continue; }
        QXLeafInfo info{};
        qx_leaf_info(lf, &info);
        if ((now - info.last_accessed_at_ms) >= idle_ms) {
            orphaned += info.size_bytes;
        }
    }
    return orphaned;
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Public C ABI
// ─────────────────────────────────────────────────────────────────────────────

extern "C" {

QX_API QXResult qx_segment_stats(
    QXSegmentHandle  segment,
    QXSegmentStats  *out_stats
) {
    if (!segment || !out_stats) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(segment->mtx);

    std::strncpy(out_stats->segment_id, segment->segment_id, 15u);
    out_stats->segment_id[15]      = '\0';
    out_stats->max_slots           = segment->max_slots;
    out_stats->active_leaves       = segment->active_count;
    out_stats->evicted_leaves      = segment->evicted_count;
    out_stats->released_leaves     = segment->released_count;
    out_stats->total_leaves        = segment->active_count
                                   + segment->evicted_count
                                   + segment->released_count;
    out_stats->used_bytes          = segment->used_bytes;
    out_stats->soft_limit_bytes    = segment->soft_limit_bytes;
    out_stats->hard_limit_bytes    = segment->hard_limit_bytes;
    out_stats->total_allocations   = segment->total_allocations;
    out_stats->total_deallocations = segment->total_deallocations;
    out_stats->total_evictions     = segment->total_evictions;
    out_stats->snapshot_at_ms      = qx_seg_now_ms();

    out_stats->hard_utilisation_pct =
        (segment->hard_limit_bytes > 0u)
        ? (static_cast<double>(segment->used_bytes)
           / static_cast<double>(segment->hard_limit_bytes)) * 100.0
        : 0.0;

    out_stats->soft_utilisation_pct =
        (segment->soft_limit_bytes > 0u)
        ? (static_cast<double>(segment->used_bytes)
           / static_cast<double>(segment->soft_limit_bytes)) * 100.0
        : 0.0;

    out_stats->pairs_ratio   =
        api_pairs_ratio(segment->total_allocations,
                        segment->total_deallocations);

    out_stats->pressure_tier =
        api_compute_tier(segment->used_bytes,
                         segment->hard_limit_bytes);

    out_stats->orphaned_bytes = api_orphaned_bytes(segment);

    return QX_OK;
}

QX_API QXResult qx_segment_id(
    QXSegmentHandle segment,
    char*           buffer
) {
    if (!segment || !buffer) { return QX_ERR_NULL_HANDLE; }

    std::lock_guard<std::mutex> lock(segment->mtx);
    std::strncpy(buffer, segment->segment_id, 15u);
    buffer[15] = '\0';
    
    return QX_OK;
}

QX_API QXResult qx_segment_used_bytes(
    QXSegmentHandle segment,
    QXSize         *out_bytes
) {
    if (!segment || !out_bytes) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(segment->mtx);
    *out_bytes = segment->used_bytes;
    return QX_OK;
}

QX_API QXResult qx_segment_soft_limit(
    QXSegmentHandle segment,
    QXSize         *out_bytes
) {
    if (!segment || !out_bytes) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(segment->mtx);
    *out_bytes = segment->soft_limit_bytes;
    return QX_OK;
}

QX_API QXResult qx_segment_hard_limit(
    QXSegmentHandle segment,
    QXSize         *out_bytes
) {
    if (!segment || !out_bytes) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(segment->mtx);
    *out_bytes = segment->hard_limit_bytes;
    return QX_OK;
}

QX_API QXResult qx_segment_pressure_tier(
    QXSegmentHandle segment,
    QXTierId       *out_tier
) {
    if (!segment || !out_tier) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(segment->mtx);
    *out_tier = api_compute_tier(segment->used_bytes,
                                  segment->hard_limit_bytes);
    return QX_OK;
}

QX_API QXResult qx_segment_pairs_ratio(
    QXSegmentHandle segment,
    double         *out_ratio
) {
    if (!segment || !out_ratio) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(segment->mtx);
    *out_ratio = api_pairs_ratio(segment->total_allocations,
                                  segment->total_deallocations);
    return QX_OK;
}

QX_API QXResult qx_segment_orphaned_bytes(
    QXSegmentHandle segment,
    QXSize         *out_bytes
) {
    if (!segment || !out_bytes) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(segment->mtx);
    *out_bytes = api_orphaned_bytes(segment);
    return QX_OK;
}

QX_API QXResult qx_segment_leaf_at_slot(
    QXSegmentHandle  segment,
    uint32_t         slot_index,
    QXLeafHandle    *out_leaf
) {
    if (!segment || !out_leaf) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(segment->mtx);
    if (slot_index >= segment->max_slots) {
        return QX_ERR_INVALID_ARGUMENT;
    }
    *out_leaf = segment->slots[slot_index];
    return QX_OK;
}

QX_API QXResult qx_segment_eviction_candidate_count(
    QXSegmentHandle segment,
    QXTierId        tier,
    uint32_t       *out_count
) {
    if (!segment || !out_count) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(segment->mtx);

    uint32_t count = 0u;
    for (uint32_t i = 0u; i < segment->max_slots; ++i) {
        QXLeafHandle lf = segment->slots[i];
        if (lf && qx_leaf_evictable_at_tier(lf, tier) == QX_TRUE) {
            ++count;
        }
    }
    *out_count = count;
    return QX_OK;
}

QX_API QXResult qx_segment_flush_terminated(
    QXSegmentHandle segment,
    uint32_t       *out_flushed
) {
    if (!segment || !out_flushed) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(segment->mtx);

    uint32_t flushed = 0u;
    for (uint32_t i = 0u; i < segment->max_slots; ++i) {
        QXLeafHandle lf = segment->slots[i];
        if (!lf) { continue; }
        QXLeafState state = QX_LEAF_STATE_ACTIVE;
        qx_leaf_state(lf, &state);
        if (state == QX_LEAF_STATE_RELEASED || state == QX_LEAF_STATE_EVICTED) {
            segment->slots[i] = nullptr;
            ++flushed;
        }
    }
    *out_flushed = flushed;
    return QX_OK;
}

QX_API QXBool qx_segment_has_capacity(QXSegmentHandle segment) {
    if (!segment) { return QX_FALSE; }
    std::lock_guard<std::mutex> lock(segment->mtx);
    return (segment->active_count < segment->max_slots)
           ? QX_TRUE : QX_FALSE;
}

QX_API QXBool qx_segment_over_soft_limit(QXSegmentHandle segment) {
    if (!segment) { return QX_FALSE; }
    std::lock_guard<std::mutex> lock(segment->mtx);
    return (segment->used_bytes > segment->soft_limit_bytes)
           ? QX_TRUE : QX_FALSE;
}

QX_API QXBool qx_segment_over_hard_limit(QXSegmentHandle segment) {
    if (!segment) { return QX_FALSE; }
    std::lock_guard<std::mutex> lock(segment->mtx);
    return (segment->used_bytes > segment->hard_limit_bytes)
           ? QX_TRUE : QX_FALSE;
}

} // extern "C"
