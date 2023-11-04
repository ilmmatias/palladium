/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _AMD64_BOOT_H_
#define _AMD64_BOOT_H_

#include <stdint.h>

#define BIOS_MEMORY_REGION_TYPE_AVAILABLE 1
#define BIOS_MEMORY_REGION_TYPE_USED 0x1000
#define BIOS_MEMORY_REGION_TYPE_KERNEL 0x1001

#define LOADER_MAGIC "BMGR"
#define LOADER_CURRENT_VERSION 0x0000

typedef struct __attribute__((packed)) {
    uint64_t BaseAddress;
    uint64_t Length;
    uint32_t Type;
} BiosMemoryRegion;

typedef struct __attribute__((packed)) {
    uint64_t VirtualAddress;
    uint64_t PhysicalAddress;
    uint64_t ImageSize;
    uint64_t EntryPoint;
    uint32_t PageFlags;
} LoaderImage;

typedef struct __attribute__((packed)) {
    char Magic[4];
    uint16_t Version;
    struct {
        uint64_t BaseAdress;
        int IsXsdt;
    } Acpi;
    struct {
        uint64_t MemorySize;
        uint64_t PageAllocatorBase;
        uint64_t PoolBitmapBase;
    } MemoryManager;
    struct {
        BiosMemoryRegion *Entries;
        uint32_t Count;
    } MemoryMap;
    struct {
        uint64_t BackBufferBase;
        uint64_t FrontBufferBase;
        uint16_t Width;
        uint16_t Height;
    } Display;
    struct {
        LoaderImage *Entries;
        uint32_t Count;
    } Images;
} LoaderBootData;

#endif /* _AMD64_BOOT_H_ */
