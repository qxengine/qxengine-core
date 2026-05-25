/* ============================================================
 * qx_memloc_enforce_internal.h
 * QXMemloc — Ai [Api] — Enforcement Internal Definitions
 *
 * Shared internal types for the four enforcement files.
 * Not part of the public API.
 *
 * ALAMTOLOGI — Quranic Science
 * Founded by Masa Bayu
 *
 * Owner : Masa Bayu
 * Date  : 2026-05-25
 * Repo  : https://github.com/qxengine/qxengine-core
 * ============================================================ */

#ifndef QX_MEMLOC_ENFORCE_INTERNAL_H
#define QX_MEMLOC_ENFORCE_INTERNAL_H

#include "qxengine/memory/qx_memloc_enforce.h"
#include "qx_memloc_flow_internal.h"
#include "qx_memloc_signal_internal.h"

#include <string.h>
#include <stdlib.h>
#include <time.h>

#if defined(__APPLE__) || defined(__linux__) || \
    defined(__ANDROID__)
#   include <pthread.h>
#endif

/* ── Internal constants ─────────────────────────────────────── */

/*
 * Minimum viable budget fraction at tier 4.
 * Class A leaves must always have at least this
 * fraction of their declared soft limit available.
 *
 * Z.1 [Pattern] — protected identity must always
 * have space to exist.
 */
#define QX_ENFORCE_MIN_VIABLE_FRACTION   0.10

/*
 * Orphan escalation thresholds in milliseconds.
 * An orphan that persists escalates in severity.
 *
 *   60s  — WARNING  (first detection)
 *   120s — CRITICAL (still not resolved)
 *   180s — FATAL    (evict regardless of class)
 *
 * Z.3 [Pairs] — water that does not return must
 * be forcibly reclaimed by the universe.
 */
#define QX_ORPHAN_WARNING_MS    ( 60u * 1000u)
#define QX_ORPHAN_CRITICAL_MS   (120u * 1000u)
#define QX_ORPHAN_FATAL_MS      (180u * 1000u)

/*
 * Equilibrium thresholds.
 * Mirror QX_EQUILIBRIUM_OVERLOAD_PCT and
 * QX_EQUILIBRIUM_STARVATION_PCT from qx_constants.h.
 */
#define QX_ENFORCE_OVERLOAD_PCT     80.0
#define QX_ENFORCE_STARVATION_PCT   20.0

/* ── Internal enforce state ─────────────────────────────────── */

/*
 * QXMemlocEnforce_s
 *
 * The internal enforcement layer state.
 * Ai [Api] holds references to both Ar [Air] and
 * An [Angin] — it reads measurements from the wind
 * and acts on the flow.
 */
struct QXMemlocEnforce_s {
    QXMemlocFlowHandle      flow;
    QXMemlocSignalHandle    signal;
    pthread_mutex_t         enforce_mutex;
    uint64_t                total_cycles;
    uint64_t                total_evictions;
    uint64_t                total_bytes_released;
    uint64_t                total_orphans_resolved;
    QXBool                  is_initialised;
};

/* ── Shared inline helpers ──────────────────────────────────── */

/*
 * enforce_now_ms
 *
 * Returns current time in milliseconds.
 */
static inline QXTimestamp enforce_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (QXTimestamp)(
        (uint64_t)ts.tv_sec  * 1000u +
        (uint64_t)ts.tv_nsec / 1000000u
    );
}

/*
 * enforce_segment_utilisation
 *
 * Returns the utilisation percentage of a segment
 * against its soft limit.
 *
 * Z.2 [Limit] — utilisation measured against the
 * constitutional soft ceiling, not the hard ceiling.
 */
static inline double enforce_segment_utilisation(
    const QXMemlocPool* pool,
    uint32_t            segment_idx)
{
    const QXPoolSegmentRegion* region;

    if (segment_idx >= QX_POOL_SEGMENT_COUNT) {
        return 0.0;
    }

    region = &pool->segments[segment_idx];

    if (region->soft_limit_bytes == 0u) {
        return 0.0;
    }

    return ((double)region->committed_bytes /
            (double)region->soft_limit_bytes) * 100.0;
}

/*
 * enforce_min_viable_budget
 *
 * Returns the minimum budget preserved for protected
 * identity under maximum pressure.
 */
static inline QXSize enforce_min_viable_budget(QXSize soft_limit_bytes)
{
    return (QXSize)(
        (double)soft_limit_bytes * QX_ENFORCE_MIN_VIABLE_FRACTION
    );
}

/*
 * enforce_orphan_stage
 *
 * Maps idle duration to orphan escalation stage.
 * 0 = none, 1 = warning, 2 = critical, 3 = fatal.
 */
static inline uint8_t enforce_orphan_stage(QXTimestamp idle_ms)
{
    if (idle_ms >= QX_ORPHAN_FATAL_MS) {
        return 3u;
    }
    if (idle_ms >= QX_ORPHAN_CRITICAL_MS) {
        return 2u;
    }
    if (idle_ms >= QX_ORPHAN_WARNING_MS) {
        return 1u;
    }
    return 0u;
}

/*
 * enforce_evict_fatal_leaf
 *
 * Evicts one leaf by its leaf record pointer.
 * Used for fatal X drift and fatal orphan eviction.
 * Decommits physical pages immediately.
 *
 * Ai [Api] — fire burns precisely.
 * One leaf. One eviction. Constitutional authority.
 *
 * Must be called with flow->leaf_mutex LOCKED.
 */
static inline void enforce_evict_fatal_leaf(
    QXMemlocFlowHandle flow,
    QXFlowLeafRecord*  rec,
    QXSize*            out_released)
{
    if (!rec->is_active) {
        return;
    }

    qx_pool_decommit(
        flow->pool,
        rec->segment_id,
        rec->committed_bytes
    );

    if (out_released != NULL) {
        *out_released += rec->committed_bytes;
    }

    rec->is_active = QX_FALSE;
    rec->ptr       = NULL;
    rec->leaf      = NULL;

    flow->total_deallocations++;
}

#endif /* QX_MEMLOC_ENFORCE_INTERNAL_H */
