/* SPDX-FileCopyrightText: (C) 2024-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _MEMORY_H_
#define _MEMORY_H_

#include <efi/spec.h>
#include <rt/list.h>

#if defined(ARCH_amd64)
#define VIRTUAL_BASE 0xFFFF900000000000
#define VIRTUAL_RANDOM_BITS 18
#define VIRTUAL_RANDOM_SHIFT 21
#define VIRTUAL_RANDOM_PAGES 512
#else
#error "Undefined ARCH for the osloader module!"
#endif

#define PAGE_TYPE_FREE 0x00
#define PAGE_TYPE_PAGE_MAP 0x01
#define PAGE_TYPE_LOADED_PROGRAM 0x02
#define PAGE_TYPE_GRAPHICS_BUFFER 0x03
#define PAGE_TYPE_OSLOADER_TEMPORARY 0x04
#define PAGE_TYPE_FIRMWARE_TEMPORARY 0x05
#define PAGE_TYPE_FIRMWARE_PERMANENT 0x06
#define PAGE_TYPE_SYSTEM_RESERVED 0x07

#define PAGE_FLAGS_WRITE 0x01
#define PAGE_FLAGS_EXEC 0x02
#define PAGE_FLAGS_DEVICE 0x04

typedef struct {
    RtSList ListHeader;
    void *Buffer;
    size_t Size;
    uint8_t Type;
} OslpAllocation;

typedef struct {
    RtDList ListHeader;
    uint8_t Type;
    uint64_t BasePage;
    uint64_t PageCount;
} OslpMemoryDescriptor;

void *OslAllocatePages(size_t Size, uint64_t Alignment, uint8_t Type);

EFI_STATUS OslpInitializeVirtualAllocator(void);
void *OslAllocateVirtualAddress(uint64_t Pages);

#endif /* _MEMORY_H_ */
