/* ============================================================
 * qx_memloc_pool.h
 * QXMemloc — Th [Tanah] — Structure Layer
 *
 * The earth beneath the water. Fixed. Constitutional.
 * Declares the two-phase memory pool that QXMemloc governs.
 *
 * ALAMTOLOGI: Tanah is the foundation that does not move.
 * The pool is declared once at startup from the manifest
 * constitutional constants and the physical truth of the
 * device RAM. It never changes during a session.
 *
 * Law of Z.1 [Pattern]     — the pool has identity
 * Law of Z.2 [Limit]       — the pool has a boundary
 *
 * Owner : Masa Bayu
 * Date  : 2026-05-25
 * Repo  : https://github.com/qxengine/qxengine-core
 * ============================================================ */

#ifndef QX_MEMLOC_POOL_H
#define QX_MEMLOC_POOL_H

#include "qxengine/core/qx_types.h"
#include "qxengine/core/qx_error.h"
#include "qxengine/core/qx_constants.h"
#include "qxengine/memory/qx_luman_init.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Constants ─────────────────────────────────────────────── */

/* Minimum pool size — soft budget floor × device scale floor  */
#define QX_POOL_MIN_BYTES       ((QXSize)(64u  * 1048576u))

/* Maximum pool size — hard ceiling for virtual reservation     */
#define QX_POOL_MAX_BYTES       ((QXSize)(2048u * 1048576u))

/* Page size — 16 KB on Apple Silicon, 4 KB elsewhere          */
#define QX_POOL_PAGE_SIZE_IOS   ((QXSize)16384u)
#define QX_POOL_PAGE_SIZE_STD   ((QXSize)4096u)

/* Segment count — nine constitutional segments                 */
#define QX_POOL_SEGMENT_COUNT   QX_SEGMENT_COUNT

/* ── Pool state ─────────────────────────────────────────────── */

/*
 * QXPoolSegmentRegion
 *
 * The constitutional boundaries of one segment within the pool.
 * Tanah — fixed at startup, never moved.
 *
 * Z.1 [Pattern]   — segment_id identifies this region
 * Z.2 [Limit]     — reserved_bytes is the hard ceiling
 */
typedef struct {
    char     segment_id[16];     /* constitutional segment identity    */
    uint8_t* base;               /* base pointer within pool           */
    QXSize   reserved_bytes;     /* virtual address space reserved     */
    QXSize   committed_bytes;    /* physical pages currently committed */
    QXSize   peak_committed;     /* peak physical usage this session   */
    QXSize   soft_limit_bytes;   /* constitutional soft ceiling        */
    QXSize   hard_limit_bytes;   /* constitutional hard ceiling        */
} QXPoolSegmentRegion;

/*
 * QXMemlocPool
 *
 * The governing pool — the single source of all memory
 * distributed by QXMemloc. Two-phase: virtual address space
 * is reserved at startup (PROT_NONE), physical pages are
 * committed on demand (PROT_READ|PROT_WRITE) and decommitted
 * under pressure (PROT_NONE).
 *
 * Ar [Air] flows through the channels defined here.
 * Th [Tanah] defines those channels.
 */
typedef struct {
    uint8_t*             base;                              /* pool base pointer                  */
    QXSize               total_reserved_bytes;              /* total virtual address reserved     */
    QXSize               total_committed_bytes;             /* total physical pages committed     */
    QXSize               total_soft_limit_bytes;            /* constitutional soft budget         */
    QXSize               total_hard_limit_bytes;            /* constitutional hard limit          */
    QXPoolSegmentRegion  segments[QX_POOL_SEGMENT_COUNT];   /* per-segment regions                */
    QXSize               page_size;                         /* platform page size                 */
    QXBool               is_initialised;                    /* pool ready flag                    */
} QXMemlocPool;

/* ── Lifecycle ──────────────────────────────────────────────── */

/*
 * qx_pool_create
 *
 * Reserves virtual address space for the constitutional pool.
 * Physical pages are NOT committed here — only virtual space
 * is reserved. This follows the two-phase commitment model:
 * reserve now, commit on demand, decommit under pressure.
 *
 * Parameters:
 *   soft_limit_bytes — total soft budget from manifest
 *   hard_limit_bytes — total hard limit from manifest
 *   device_scale     — device RAM scale factor (0.60/0.75/1.00)
 *   out_pool         — caller-owned pool struct to initialise
 *
 * Returns QX_OK on success.
 * Returns QX_ERR_INVALID_ARGUMENT if limits are below minimums.
 * Returns QX_ERR_INTERNAL if mmap fails.
 *
 * Thread safety: NOT thread-safe. Call once at engine startup.
 */
QX_API QXResult qx_pool_create(
    QXSize        soft_limit_bytes,
    QXSize        hard_limit_bytes,
    double        device_scale,
    QXMemlocPool* out_pool
);

QX_API QXResult qx_pool_create_luman(
    const QXLumanInitResult* luman_result,
    QXMemlocPool*            out_pool
);

/*
 * qx_pool_destroy
 *
 * Releases all virtual address space and resets the pool.
 * Z.3 [Pairs] — the pool was created, it must be destroyed.
 *
 * Thread safety: NOT thread-safe. Call once at engine shutdown.
 */
QX_API void qx_pool_destroy(QXMemlocPool* pool);

/* ── Commitment ─────────────────────────────────────────────── */

/*
 * qx_pool_commit
 *
 * Commits physical pages for a segment up to the requested
 * size. Pages are committed in page_size increments.
 * Ar [Air] — water flows into the channel when needed.
 *
 * Returns QX_ERR_HARD_BUDGET if commitment would exceed
 * the segment hard limit.
 *
 * Thread safety: Thread-safe per segment.
 */
QX_API QXResult qx_pool_commit(
    QXMemlocPool* pool,
    const char*   segment_id,
    QXSize        additional_bytes
);

/*
 * qx_pool_decommit
 *
 * Returns physical pages to the OS for a segment.
 * Pages are decommitted via mprotect(PROT_NONE).
 * Physical RAM is returned to the OS immediately.
 * Ar [Air] — water returns to the river under drought.
 *
 * Thread safety: Thread-safe per segment.
 */
QX_API QXResult qx_pool_decommit(
    QXMemlocPool* pool,
    const char*   segment_id,
    QXSize        release_bytes
);

/* ── Query ──────────────────────────────────────────────────── */

/*
 * qx_pool_segment_region
 *
 * Returns a read-only pointer to a segment's region.
 * Returns NULL if segment_id is not found.
 */
QX_API const QXPoolSegmentRegion* qx_pool_segment_region(
    const QXMemlocPool* pool,
    const char*         segment_id
);

/*
 * qx_pool_utilisation_pct
 *
 * Returns the current physical commitment as a percentage
 * of the soft limit. Used by Law Z.2 [Limit] enforcement.
 */
QX_API double qx_pool_utilisation_pct(
    const QXMemlocPool* pool,
    const char*         segment_id
);

/*
 * qx_pool_total_utilisation_pct
 *
 * Returns total pool utilisation across all segments.
 * Used by the cognitive cycle and pressure tier evaluation.
 */
QX_API double qx_pool_total_utilisation_pct(
    const QXMemlocPool* pool
);

#ifdef __cplusplus
}
#endif

#endif /* QX_MEMLOC_POOL_H */
