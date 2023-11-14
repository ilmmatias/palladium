/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _BOOT_H_
#define _BOOT_H_

#include <stdint.h>

#define BOOT_MD_FREE 0
#define BOOT_MD_HARDWARE 1
#define BOOT_MD_BOOTMGR 2
#define BOOT_MD_KERNEL 3

#define LOADER_MAGIC "BMGR"
#define LOADER_CURRENT_VERSION 0x0000

typedef struct __attribute__((packed)) {
    int Type;
    uint64_t BaseAddress;
    uint64_t Length;
} BootMemoryRegion;

typedef struct __attribute__((packed)) {
    uint64_t VirtualAddress;
    uint64_t PhysicalAddress;
    uint64_t ImageSize;
    uint64_t EntryPoint;
    uint32_t PageFlags;
} BootLoaderImage;

typedef struct __attribute__((packed)) {
    char Magic[4];
    uint16_t Version;
    struct {
        uint64_t BaseAdress;
        int TableType;
    } Acpi;
    struct {
        uint64_t MemorySize;
        uint64_t PageAllocatorBase;
        uint64_t PoolBitmapBase;
    } MemoryManager;
    struct {
        BootMemoryRegion *Entries;
        uint32_t Count;
    } MemoryMap;
    struct {
        uint64_t BackBufferBase;
        uint64_t FrontBufferBase;
        uint16_t Width;
        uint16_t Height;
        uint16_t Pitch;
    } Display;
    struct {
        BootLoaderImage *Entries;
        uint32_t Count;
    } Images;
} LoaderBootData;

#endif /* _BOOT_H_ */
