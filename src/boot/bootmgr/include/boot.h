/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _BOOT_H_
#define _BOOT_H_

#include <stddef.h>
#include <stdint.h>

#define PAGE_WRITE 0x01
#define PAGE_EXEC 0x02

#define LOADER_MAGIC "BMGR"
#define LOADER_CURRENT_VERSION 0x0000

typedef struct __attribute__((packed)) {
    uint64_t VirtualAddress;
    uint64_t PhysicalAddress;
    uint64_t ImageSize;
    uint64_t EntryPoint;
    int* PageFlags;
} LoadedImage;

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
        uint64_t BaseAddress;
        uint32_t Count;
    } MemoryMap;
    struct {
        uint64_t BackBufferBase;
        uint64_t FrontBufferBase;
        uint16_t Width;
        uint16_t Height;
    } Display;
    struct {
        uint64_t BaseAddress;
        uint32_t Count;
    } Images;
} LoaderBootData;

/* The following parameter should always follow sizeof(HalpProcessor). */
#if defined(ARCH_x86) || defined(ARCH_amd64)
#define SIZEOF_PROCESSOR 0x5E44
#else
#error "Undefined ARCH for the bootmgr module!"
#endif /* ARCH */

void BiInitArch(void* BootBlock);

void BiLoadMenuEntries(void);
[[noreturn]] void BiEnterMenu(void);
[[noreturn]] void BiLoadPalladium(const char* SystemFolder);

void BiCheckCompatibility(void);
[[noreturn]] void BiTransferExecution(LoadedImage* Images, size_t ImageCount);

[[noreturn]] void BmPanic(const char* Message);

#endif /* _BOOT_H_ */
