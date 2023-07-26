/* SPDX-FileCopyrightText: (C) 2023 yuuma03
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _BIOS_H_
#define _BIOS_H_

#include <stdint.h>

#define BIOS_MEMORY_REGION_TYPE_AVAILABLE 1
#define BIOS_MEMORY_REGION_TYPE_BOOTMANAGER 0x1000

typedef struct __attribute__((packed)) {
    uint8_t BootDrive;
    uint32_t MemoryCount;
    uint32_t MemoryRegions;
} BiosBootBlock;

typedef struct __attribute__((packed)) {
    uint64_t BaseAddress;
    uint64_t Length;
    uint32_t Type;
    uint32_t UsedPages;
} BiosMemoryRegion;

#endif /* _BIOS_H_ */
