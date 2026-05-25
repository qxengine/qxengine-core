// =============================================================================
// QXEngine Core – src/memory/qx_segment.cpp
// Owner : Masa Bayu
// Created: 2026-05-24
// Repo   : https://github.com/qxengine/qxengine-core
// Description: Internal factory and mutation functions for QXSegment.
//              Implements qx::internal::segment_* functions consumed by
//              QXMemloc and QXPressureCoordinator.
// =============================================================================

#include "qx_segment_internal.h"

#include <cstring>
#include <cstdio>
#include <new>

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

namespace {

[[maybe_unused]] static QXTierId compute_tier(QXSize used_bytes,
                              QXSize hard_limit_bytes) noexcept {
    if (hard_limit_bytes == 0u) { return QX_TIER_NORMAL; }
    const double u = static_cast<double>(used_bytes)
                   / static_cast<double>(hard_limit_bytes);
    if (u >= QX_TIER_4_THRESHOLD_PCT / 100.0) { return QX_TIER_CRITICAL;  }
    if (u >= QX_TIER_3_THRESHOLD_PCT / 100.0) { return QX_TIER_HIGH;      }
    if (u >= QX_TIER_2_THRESHOLD_PCT / 100.0) { return QX_TIER_ELEVATED;  }
    return QX_TIER_NORMAL;
}

[[maybe_unused]] static double compute_pairs_ratio(uint64_t allocs,
                                   uint64_t deallocs) noexcept {
    if (allocs == 0u) { return 1.0; }
    const double ratio = static_cast<double>(deallocs)
                       / static_cast<double>(allocs);
    return (ratio > 1.0) ? 1.0 : ratio;
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – qx::internal implementations
// ─────────────────────────────────────────────────────────────────────────────

namespace qx::internal {

QXResult segment_create(
    const char       *segment_id,
    uint32_t          max_slots,
    QXSize            soft_limit_bytes,
    QXSize            hard_limit_bytes,
    QXSegmentHandle  *out_segment
) noexcept {
    if (!segment_id || !out_segment)        { return QX_ERR_NULL_HANDLE;      }
    if (max_slots == 0u ||
        max_slots > QX_SLOTS_HIGH_PRIORITY) { return QX_ERR_INVALID_ARGUMENT; }
    if (hard_limit_bytes <= soft_limit_bytes) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    auto *seg = new (std::nothrow) QXSegment_s{};
    if (!seg) { return QX_ERR_INTERNAL; }

    std::strncpy(seg->segment_id, segment_id, 15u);
    seg->segment_id[15]      = '\0';
    seg->max_slots           = max_slots;
    seg->soft_limit_bytes    = soft_limit_bytes;
    seg->hard_limit_bytes    = hard_limit_bytes;
    seg->used_bytes          = 0u;
    seg->active_count        = 0u;
    seg->evicted_count       = 0u;
    seg->released_count      = 0u;
    seg->total_allocations   = 0u;
    seg->total_deallocations = 0u;
    seg->total_evictions     = 0u;

    for (uint32_t i = 0u; i < QX_SLOTS_HIGH_PRIORITY; ++i) {
        seg->slots[i] = nullptr;
    }

    *out_segment = seg;
    return QX_OK;
}

void segment_destroy(QXSegmentHandle segment) noexcept {
    delete segment;
}

QXResult segment_allocate(
    QXSegmentHandle segment,
    QXLeafHandle    leaf,
    QXSize          size_bytes,
    uint32_t       *out_slot
) noexcept {
    if (!segment || !leaf || !out_slot) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(segment->mtx);

    // Law 2 – hard budget check
    if (segment->used_bytes + size_bytes > segment->hard_limit_bytes) {
        return QX_ERR_HARD_BUDGET;
    }

    for (uint32_t i = 0u; i < segment->max_slots; ++i) {
        if (segment->slots[i] == nullptr) {
            segment->slots[i]     = leaf;
            segment->used_bytes  += size_bytes;
            segment->active_count++;
            segment->total_allocations++;
            *out_slot = i;
            return QX_OK;
        }
    }
    return QX_ERR_NO_SLOT;
}

QXResult segment_deallocate(
    QXSegmentHandle segment,
    QXLeafHandle    leaf,
    QXSize         *out_bytes
) noexcept {
    if (!segment || !leaf || !out_bytes) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(segment->mtx);

    for (uint32_t i = 0u; i < segment->max_slots; ++i) {
        if (segment->slots[i] == leaf) {
            QXSize freed = 0u;
            qx_leaf_size_bytes(leaf, &freed);

            segment->slots[i]     = nullptr;
            segment->used_bytes  -= (freed <= segment->used_bytes)
                                    ? freed : segment->used_bytes;
            segment->active_count    = (segment->active_count > 0u)
                                     ? segment->active_count - 1u : 0u;
            segment->released_count++;
            segment->total_deallocations++;
            *out_bytes = freed;
            return QX_OK;
        }
    }
    return QX_ERR_LEAF_NOT_FOUND;
}

QXResult segment_evict(
    QXSegmentHandle segment,
    QXLeafHandle    leaf,
    QXSize         *out_bytes
) noexcept {
    if (!segment || !leaf || !out_bytes) { return QX_ERR_NULL_HANDLE; }
    std::lock_guard<std::mutex> lock(segment->mtx);

    for (uint32_t i = 0u; i < segment->max_slots; ++i) {
        if (segment->slots[i] == leaf) {
            QXSize freed = 0u;
            qx_leaf_size_bytes(leaf, &freed);

            segment->slots[i]    = nullptr;
            segment->used_bytes -= (freed <= segment->used_bytes)
                                   ? freed : segment->used_bytes;
            segment->active_count   = (segment->active_count > 0u)
                                    ? segment->active_count - 1u : 0u;
            segment->evicted_count++;
            segment->total_evictions++;
            *out_bytes = freed;
            return QX_OK;
        }
    }
    return QX_ERR_LEAF_NOT_FOUND;
}

QXResult segment_eviction_candidates(
    QXSegmentHandle segment,
    QXTierId        tier,
    QXLeafHandle   *out_leaves,
    uint32_t        capacity,
    uint32_t       *out_count
) noexcept {
    if (!segment || !out_leaves || !out_count) {
        return QX_ERR_NULL_HANDLE;
    }
    std::lock_guard<std::mutex> lock(segment->mtx);

    uint32_t found = 0u;

    for (uint32_t i = 0u;
         i < segment->max_slots && found < capacity; ++i)
    {
        QXLeafHandle lf = segment->slots[i];
        if (!lf) { continue; }
        if (qx_leaf_evictable_at_tier(lf, tier) == QX_TRUE) {
            out_leaves[found++] = lf;
        }
    }

    // LRU-first sort (insertion sort – max 7 elements)
    for (uint32_t a = 1u; a < found; ++a) {
        QXLeafHandle key = out_leaves[a];
        QXLeafInfo   key_info{};
        qx_leaf_info(key, &key_info);

        int32_t b = static_cast<int32_t>(a) - 1;
        while (b >= 0) {
            QXLeafInfo b_info{};
            qx_leaf_info(out_leaves[b], &b_info);
            if (b_info.last_accessed_at_ms <= key_info.last_accessed_at_ms) {
                break;
            }
            out_leaves[b + 1] = out_leaves[b];
            --b;
        }
        out_leaves[b + 1] = key;
    }

    *out_count = found;
    return QX_OK;
}

} // namespace qx::internal
