/* SPDX-FileCopyrightText: (C) 2024-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _MEMORY_H_
#define _MEMORY_H_

#include <efi/spec.h>
#include <rt/list.h>

#if defined(ARCH_amd64)
#define ARENA_ENTRIES 512
#define ARENA_PAGE_SIZE 0x40000000
#define ARENA_BASE 0xFFFF900000000000
#else
#error "Undefined ARCH for the osloader module!"
#endif

#define PAGE_FREE 0x00
#define PAGE_LOW_MEMORY 0x01
#define PAGE_LOADED_PROGRAM 0x02
#define PAGE_OSLOADER 0x03
#define PAGE_BOOT_PROCESSOR 0x04
#define PAGE_BOOT_TABLES 0x05
#define PAGE_FIRMWARE_TEMPORARY 0x06
#define PAGE_FIRMWARE_PERMANENT 0x07
#define PAGE_SYSTEM_RESERVED 0x08

#define PAGE_FLAGS_WRITE 0x01
#define PAGE_FLAGS_EXEC 0x02

typedef struct {
    RtDList ListHeader;
    uint8_t Type;
    uint64_t BasePage;
    uint64_t PageCount;
} OslpMemoryDescriptor;

typedef struct OslpMemoryArenaEntry {
    uint64_t Base;
    struct OslpMemoryArenaEntry *Next;
} OslpMemoryArenaEntry;

EFI_STATUS OslpInitializeMemoryMap(void);
void *OslAllocatePages(uint64_t Size, uint8_t Type);
void OslFreePages(void *Base, uint64_t Size);

void OslpInitializeEntropy(void);
void OslpInitializeVirtualAllocator(void);
void *OslAllocateVirtualAddress(uint64_t Pages);

#endif /* _MEMORY_H_ */
