/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _MI_H_
#define _MI_H_

#include <ki.h>
#include <mm.h>

#ifdef ARCH_amd64
#define MI_POOL_START 0xFFFF908000000000
#define MI_POOL_SIZE 0x2000000000
#else
#error "Undefined ARCH for the kernel module!"
#endif /* ARCH */

#define MI_MAP_WRITE 0x01
#define MI_MAP_EXEC 0x02
#define MI_MAP_DEVICE 0x04

#define MI_PAGE_FREE 0x00
#define MI_PAGE_LOW_MEMORY 0x01
#define MI_PAGE_LOADED_PROGRAM 0x02
#define MI_PAGE_OSLOADER 0x03
#define MI_PAGE_BOOT_PROCESSOR 0x04
#define MI_PAGE_BOOT_TABLES 0x05
#define MI_PAGE_FIRMWARE_TEMPORARY 0x06
#define MI_PAGE_FIRMWARE_PERMANENT 0x07
#define MI_PAGE_SYSTEM_RESERVED 0x08

#define MI_PAGE_BASE(Entry) ((uint64_t)((Entry) - MiPageList) << MM_PAGE_SHIFT)

typedef struct {
    RtDList ListHeader;
    uint8_t Type;
    uint32_t BasePage;
    uint32_t PageCount;
} MiMemoryDescriptor;

typedef struct __attribute__((packed)) MiPageEntry {
    struct MiPageEntry *NextPage;
    uint32_t References;
    uint32_t Pages;
} MiPageEntry;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void MiInitializePageAllocator(KiLoaderBlock *LoaderBlock);
void MiInitializePool(KiLoaderBlock *LoaderBlock);

void *MiEnsureEarlySpace(uint64_t PhysicalAddress, size_t Size);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _MI_H_ */
