/**
 * @file qx_platform_ram_host.c
 * @brief Platform RAM Detection — Host
 *
 * ALAMTOLOGI — QURANIC SCIENCE
 * Founder: Masa Bayu
 *
 * POSIX host implementation for CI, macOS, and Linux builds.
 *
 * Repository: https://github.com/qxengine/qxengine-core
 * Date:       2026-05-25
 * Version:    1.0.0-rc.3
 */

#include "platform/qx_platform_ram.h"

#include <unistd.h>

uint64_t qx_platform_ram_bytes(void)
{
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);

    if (pages <= 0 || page_size <= 0) {
        return 0u;
    }

    return (uint64_t)pages * (uint64_t)page_size;
}
