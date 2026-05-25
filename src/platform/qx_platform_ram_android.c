/**
 * @file qx_platform_ram_android.c
 * @brief Platform RAM Detection — Android
 *
 * ALAMTOLOGI — QURANIC SCIENCE
 * Founder: Masa Bayu
 *
 * Reads /proc/meminfo and extracts MemTotal.
 *
 * Repository: https://github.com/qxengine/qxengine-core
 * Date:       2026-05-25
 * Version:    1.0.0-rc.3
 */

#include "platform/qx_platform_ram.h"

#include <stdio.h>
#include <string.h>

uint64_t qx_platform_ram_bytes(void)
{
    FILE* f = fopen("/proc/meminfo", "r");
    char line[128];
    uint64_t total_kb = 0u;

    if (f == NULL) {
        return 0u;
    }

    while (fgets(line, sizeof(line), f) != NULL) {
        if (strncmp(line, "MemTotal:", 9u) == 0) {
            unsigned long long kb = 0u;
            if (sscanf(line + 9, " %llu", &kb) == 1) {
                total_kb = (uint64_t)kb;
            }
            break;
        }
    }

    fclose(f);
    return total_kb * 1024ULL;
}
