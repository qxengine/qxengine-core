/* ============================================================
 * qx_memloc_flow_internal.h
 * QXMemloc — Ar [Air] — Flow Layer Internal Definitions
 *
 * Shared internal types and helpers for the four flow
 * implementation files. Not part of the public API.
 *
 * ALAMTOLOGI — Quranic Science
 * Founded by Masa Bayu
 *
 * Owner : Masa Bayu
 * Date  : 2026-05-25
 * Repo  : https://github.com/qxengine/qxengine-core
 * ============================================================ */

#ifndef QX_MEMLOC_FLOW_INTERNAL_H
#define QX_MEMLOC_FLOW_INTERNAL_H

#include "qxengine/memory/qx_memloc_flow.h"

#include <string.h>
#include <stdlib.h>
#include <time.h>

#if defined(__APPLE__) || defined(__linux__) || \
    defined(__ANDROID__)
#   include <pthread.h>
#   include <sys/mman.h>
#endif

/* ── Internal constants ─────────────────────────────────────── */

#define QX_FLOW_MAX_LEAVES          256u
#define QX_FLOW_MAX_CALLBACKS       128u
#define QX_FLOW_MAX_USAGE_REPORTS   128u

#define QX_SLAB_CLASS_SMALL         (    1u * 1024u)
#define QX_SLAB_CLASS_MEDIUM        (   64u * 1024u)
#define QX_SLAB_CLASS_LARGE         (    1u * 1048576u)
#define QX_SLAB_CLASS_HUGE          (   16u * 1048576u)

#define QX_LIFECYCLE_FOREGROUND     0u
#define QX_LIFECYCLE_BACKGROUND     1u

#define QX_PRESSURE_SCALE_TIER1     0.70
#define QX_PRESSURE_SCALE_TIER2     0.50
#define QX_PRESSURE_SCALE_TIER3     0.25
#define QX_PRESSURE_SCALE_TIER4     0.10
#define QX_BACKGROUND_SCALE         0.25

/* ── Internal types ─────────────────────────────────────────── */

/*
 * QXFlowLeafRecord
 *
 * Internal record of one active allocation.
 * Tracks the constitutional state of one drop of water
 * from birth to return.
 *
 * Z.1 [Pattern]  — label and segment identity
 * Z.2 [Limit]    — size_bytes and segment_idx
 * Z.3 [Pairs]    — allocated_at_ms, is_active flag
 */
typedef struct {
    char          label[128];
    char          segment_id[16];
    uint32_t      segment_idx;
    QXLeafClassId leaf_class;
    QXSize        size_bytes;
    QXSize        committed_bytes;
    void*         ptr;
    QXLeafHandle  leaf;
    QXTimestamp   allocated_at_ms;
    QXTimestamp   last_touched_ms;
    QXBool        is_active;
    uint32_t      slot_index;
} QXFlowLeafRecord;

/*
 * QXFlowCallbackRecord
 *
 * One registered budget callback from a subsystem.
 */
typedef struct {
    char             slot_id[64];
    QXBudgetCallback callback;
    void*            user_data;
    QXSize           current_limit;
    QXBool           is_registered;
} QXFlowCallbackRecord;

/*
 * QXMemlocFlow_s
 *
 * The internal flow controller state.
 * All flow state lives here — no globals.
 */
struct QXMemlocFlow_s {
    QXMemlocPool*          pool;

    QXFlowLeafRecord       leaves[QX_FLOW_MAX_LEAVES];
    uint32_t               leaf_count;
    pthread_mutex_t        leaf_mutex;

    QXFlowCallbackRecord   callbacks[QX_FLOW_MAX_CALLBACKS];
    uint32_t               callback_count;
    pthread_mutex_t        callback_mutex;

    QXUsageReport          usage_reports[QX_FLOW_MAX_USAGE_REPORTS];
    uint32_t               usage_report_count;
    pthread_mutex_t        usage_mutex;

    uint8_t                lifecycle_state;
    uint8_t                pressure_tier;
    char                   screen_profile_id[64];

    uint64_t               total_allocations;
    uint64_t               total_deallocations;
    uint64_t               total_bytes_allocated;
    uint64_t               total_bytes_deallocated;
    uint64_t               unlabelled_rejections;

    QXBool                 is_initialised;
};

/* ── Cross-file internal functions ─────────────────────────── */

uint32_t flow_evict_by_class(
    struct QXMemlocFlow_s* flow,
    QXLeafClassId          evict_class,
    const char*            segment_id,
    QXSize*                out_bytes_released
);

void flow_fire_callbacks(
    struct QXMemlocFlow_s* flow,
    const char*            segment_id,
    QXSize                 new_limit_bytes,
    QXSize                 previous_limit_bytes,
    uint8_t                pressure_tier,
    uint8_t                lifecycle_state,
    QXTimestamp            updated_at_ms
);

/* ── Shared internal helpers ────────────────────────────────── */

/*
 * flow_now_ms
 *
 * Returns current time in milliseconds since Unix epoch.
 */
static inline QXTimestamp flow_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (QXTimestamp)(
        (uint64_t)ts.tv_sec  * 1000u +
        (uint64_t)ts.tv_nsec / 1000000u
    );
}

/*
 * flow_slab_round_up
 *
 * Rounds size up to the nearest slab class.
 * Z.2 [Limit] — known, constitutional size classes.
 */
static inline QXSize flow_slab_round_up(QXSize size_bytes)
{
    if (size_bytes == 0u) {
        return QX_SLAB_CLASS_SMALL;
    }
    if (size_bytes <= QX_SLAB_CLASS_SMALL)  {
        return QX_SLAB_CLASS_SMALL;
    }
    if (size_bytes <= QX_SLAB_CLASS_MEDIUM) {
        return QX_SLAB_CLASS_MEDIUM;
    }
    if (size_bytes <= QX_SLAB_CLASS_LARGE)  {
        return QX_SLAB_CLASS_LARGE;
    }
    if (size_bytes <= QX_SLAB_CLASS_HUGE)   {
        return QX_SLAB_CLASS_HUGE;
    }
    return ((size_bytes + QX_SLAB_CLASS_HUGE - 1u) /
             QX_SLAB_CLASS_HUGE) * QX_SLAB_CLASS_HUGE;
}

/*
 * flow_find_segment_index
 *
 * Returns the pool segment index for a given segment_id.
 * Returns QX_POOL_SEGMENT_COUNT if not found.
 */
static inline uint32_t flow_find_segment_index(
    const char* segment_id)
{
    static const char* ids[QX_POOL_SEGMENT_COUNT] = {
        "QLM-CORE", "QLM-UI",  "QLM-DATA",
        "QLM-IMG",  "QLM-NET", "QLM-AI",
        "QLM-SEC",  "QLM-LOG", "QLM-TEMP"
    };
    uint32_t i;
    if (segment_id == NULL) {
        return QX_POOL_SEGMENT_COUNT;
    }
    for (i = 0u; i < QX_POOL_SEGMENT_COUNT; ++i) {
        if (strncmp(segment_id, ids[i], 16u) == 0) {
            return i;
        }
    }
    return QX_POOL_SEGMENT_COUNT;
}

/*
 * flow_find_leaf_by_handle
 *
 * Finds an active leaf record by its QXLeafHandle.
 * Returns NULL if not found or inactive.
 * Must be called with leaf_mutex LOCKED.
 */
static inline QXFlowLeafRecord* flow_find_leaf_by_handle(
    struct QXMemlocFlow_s* flow,
    QXLeafHandle           leaf)
{
    uint32_t i;
    if (leaf == NULL) {
        return NULL;
    }
    for (i = 0u; i < QX_FLOW_MAX_LEAVES; ++i) {
        if (flow->leaves[i].is_active &&
            flow->leaves[i].leaf == leaf) {
            return &flow->leaves[i];
        }
    }
    return NULL;
}

/*
 * flow_compute_pressure_scale
 *
 * Returns budget scale factor for pressure tier
 * and lifecycle state.
 */
static inline double flow_compute_pressure_scale(
    uint8_t pressure_tier,
    uint8_t lifecycle_state)
{
    double scale = 1.0;
    switch (pressure_tier) {
        case 1u: scale = QX_PRESSURE_SCALE_TIER1; break;
        case 2u: scale = QX_PRESSURE_SCALE_TIER2; break;
        case 3u: scale = QX_PRESSURE_SCALE_TIER3; break;
        case 4u: scale = QX_PRESSURE_SCALE_TIER4; break;
        default: scale = 1.0;                     break;
    }
    if (lifecycle_state == QX_LIFECYCLE_BACKGROUND) {
        scale *= QX_BACKGROUND_SCALE;
    }
    return scale;
}

#endif /* QX_MEMLOC_FLOW_INTERNAL_H */
