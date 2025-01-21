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
#define MI_DESCR_PAGE_MAP 0x01
#define MI_DESCR_LOADED_PROGRAM 0x02
#define MI_DESCR_GRAPHICS_BUFFER 0x03
#define MI_DESCR_OSLOADER_TEMPORARY 0x04
#define MI_DESCR_FIRMWARE_TEMPORARY 0x05
#define MI_DESCR_FIRMWARE_PERMANENT 0x06
#define MI_DESCR_SYSTEM_RESERVED 0x07

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

void MiInitializePageAllocator(KiLoaderBlock *LoaderBlock);
void MiInitializePool(KiLoaderBlock *LoaderBlock);
void MiReleaseBootRegions(void);

void *MiEnsureEarlySpace(uint64_t PhysicalAddress, size_t Size);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _MI_H_ */
