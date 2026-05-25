/* ============================================================
 * qx_memloc_pool_internal.h
 * QXMemloc — Th [Tanah] — Pool Internal Helpers
 *
 * Shared internal constants, platform includes, and helper
 * declarations for the split pool implementation.
 *
 * ALAMTOLOGI — Quranic Science
 * Founded by Masa Bayu
 *
 * Owner : Masa Bayu
 * Date  : 2026-05-25
 * Repo  : https://github.com/qxengine/qxengine-core
 * ============================================================ */

#ifndef QX_MEMLOC_POOL_INTERNAL_H
#define QX_MEMLOC_POOL_INTERNAL_H

#include "qxengine/memory/qx_memloc_pool.h"

#include <string.h>

#if defined(__APPLE__) || defined(__ANDROID__) || defined(__linux__)
#   include <pthread.h>
#   include <sys/mman.h>
#   include <unistd.h>
#else
#   error "QXMemloc pool requires a POSIX platform (iOS, Android, Linux, macOS)"
#endif

/* Hard limit is this multiple of soft limit per segment. */
#define QX_HARD_TO_SOFT_RATIO   1.5

extern const double      qx_pool_segment_weights[QX_POOL_SEGMENT_COUNT];
extern const char* const qx_pool_segment_ids[QX_POOL_SEGMENT_COUNT];
extern pthread_mutex_t   qx_pool_segment_mutexes[QX_POOL_SEGMENT_COUNT];
extern int               qx_pool_mutexes_initialised;

QXSize qx_pool_platform_page_size(void);
QXSize qx_pool_align_to_page(QXSize size_bytes, QXSize page_size);
uint32_t qx_pool_find_segment_index(const char* segment_id);
QXResult qx_pool_init_segment_mutexes(void);
void qx_pool_destroy_segment_mutexes(void);
QXResult qx_pool_validate_pool_limits(
    QXSize soft_limit_bytes,
    QXSize hard_limit_bytes
);
QXResult qx_pool_validate_device_scale(double device_scale);

#endif /* QX_MEMLOC_POOL_INTERNAL_H */
