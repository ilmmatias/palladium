/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
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

#define MI_DESCR_FREE 0x00
#define MI_DESCR_LOW_MEMORY 0x01
#define MI_DESCR_LOADED_PROGRAM 0x02
#define MI_DESCR_OSLOADER 0x03
#define MI_DESCR_BOOT_PROCESSOR 0x04
#define MI_DESCR_BOOT_TABLES 0x05
#define MI_DESCR_FIRMWARE_TEMPORARY 0x06
#define MI_DESCR_FIRMWARE_PERMANENT 0x07
#define MI_DESCR_SYSTEM_RESERVED 0x08

#define MI_PAGE_FLAGS_USED 0x01
#define MI_PAGE_FLAGS_CONTIG_BASE 0x02
#define MI_PAGE_FLAGS_CONTIG_ITEM 0x04
#define MI_PAGE_FLAGS_CONTIG_ANY (MI_PAGE_FLAGS_CONTIG_BASE | MI_PAGE_FLAGS_CONTIG_ITEM)
#define MI_PAGE_FLAGS_POOL_BASE 0x08
#define MI_PAGE_FLAGS_POOL_ITEM 0x10
#define MI_PAGE_FLAGS_POOL_ANY (MI_PAGE_FLAGS_POOL_BASE | MI_PAGE_FLAGS_POOL_ITEM)

#define MI_PAGE_ENTRY(Base) (MiPageList[(uint64_t)(Base) >> MM_PAGE_SHIFT])
#define MI_PAGE_BASE(Entry) ((uint64_t)((Entry) - MiPageList) << MM_PAGE_SHIFT)

typedef struct {
    RtDList ListHeader;
    uint8_t Type;
    uint64_t BasePage;
    uint64_t PageCount;
} MiMemoryDescriptor;

typedef struct MiPageEntry {
    uint16_t Flags;
    union {
        RtDList ListHeader;
        uint64_t Pages;
    };
} MiPageEntry;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void MiSaveMemoryDescriptors(KiLoaderBlock *KiLoaderBlock);
void MiInitializePageAllocator(void);
void MiInitializePool(void);
void MiReleaseBootRegions(void);

void *MiEnsureEarlySpace(uint64_t PhysicalAddress, size_t Size);

void *MiEarlyAllocatePages(KiLoaderBlock *LoaderBlock, uint64_t Pages);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _MI_H_ */
