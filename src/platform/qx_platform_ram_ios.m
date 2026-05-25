/**
 * @file qx_platform_ram_ios.m
 * @brief Platform RAM Detection — iOS / iPadOS
 *
 * ALAMTOLOGI — QURANIC SCIENCE
 * Founder: Masa Bayu
 *
 * Uses NSProcessInfo.physicalMemory for Apple mobile targets.
 *
 * Repository: https://github.com/qxengine/qxengine-core
 * Date:       2026-05-25
 * Version:    1.0.0-rc.3
 */

#import <Foundation/Foundation.h>

#include "platform/qx_platform_ram.h"

uint64_t qx_platform_ram_bytes(void)
{
    return (uint64_t)[[NSProcessInfo processInfo] physicalMemory];
}
