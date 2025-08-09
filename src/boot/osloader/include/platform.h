/* SPDX-FileCopyrightText: (C) 2024-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include <memory.h>

#if __has_include(ARCH_MAKE_INCLUDE_PATH_WITHOUT_PREFIX(platform.h))
#include ARCH_MAKE_INCLUDE_PATH_WITHOUT_PREFIX(platform.h)
#else
#error "Undefined ARCH for the CRT module!"
#endif /* __has_include */

#define OSLP_BOOT_MAGIC "OLDR"
#define OSLP_BOOT_VERSION 0x0000'0000'00000004

typedef struct __attribute__((packed)) {
    char Magic[4];
    uint64_t LoaderVersion;
    RtDList *MemoryDescriptorListHead;
    RtDList *BootDriverListHead;
} OslpBootBasicData;

typedef struct __attribute__((packed)) {
    uint32_t Version;
    void *RootPointer;
    void *RootTable;
    uint32_t RootTableSize;
} OslpBootAcpiData;

typedef struct __attribute__((packed)) {
    void *BackBuffer;
    void *FrontBuffer;
    uint32_t Width;
    uint32_t Height;
    uint32_t Pitch;
} OslpBootGraphicsData;

typedef struct {
    OslpBootBasicData Basic;
    OslpBootAcpiData Acpi;
    OslpBootGraphicsData Graphics;
    OslpBootArchData Arch;
} OslpBootBlock;

bool OslpCreateMemoryDescriptors(
    RtDList *LoadedPrograms,
    void *FrontBuffer,
    UINTN FrameBufferSize,
    RtDList **MemoryDescriptorListHead,
    RtDList *MemoryDescriptorStack,
    UINTN *MemoryMapSize,
    EFI_MEMORY_DESCRIPTOR **MemoryMap,
    UINTN *DescriptorSize,
    UINT32 *DescriptorVersion);

bool OslpUpdateMemoryDescriptors(
    RtDList *MemoryDescriptorListHead,
    RtDList *MemoryDescriptorStack,
    uint8_t Type,
    uint64_t BasePage,
    uint64_t PageCount);

void *OslpCreatePageMap(
    RtDList *MemoryDescriptorListHead,
    RtDList *MemoryDescriptorStack,
    RtDList *LoadedPrograms,
    uint64_t FrameBufferSize,
    void *BackBufferPhys,
    void *BackBufferVirt,
    void *FrontBufferPhys,
    void *FrontBufferVirt);

[[noreturn]] void OslpTransferExecution(
    OslpBootBlock *BootBlock,
    void *BootStack,
    void *PageMap,
    UINTN MemoryMapSize,
    EFI_MEMORY_DESCRIPTOR *MemoryMap,
    UINTN DescriptorSize,
    UINT32 DescriptorVersion);

#endif /* _PLATFORM_H_ */
