/* SPDX-FileCopyrightText: (C) 2024-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include <memory.h>

#define OSLP_BOOT_MAGIC "OLDR"
#define OSLP_BOOT_VERSION 0x0000'0000'00000002

typedef struct {
    char Magic[4];
    uint64_t LoaderVersion;
    RtDList *MemoryDescriptorListHead;
    RtDList *BootDriverListHead;
    void *AcpiTable;
    uint32_t AcpiTableVersion;
    void *BackBuffer;
    void *FrontBuffer;
    uint32_t FramebufferWidth;
    uint32_t FramebufferHeight;
    uint32_t FramebufferPitch;
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
    void *BackBuffer);

[[noreturn]] void OslpTransferExecution(
    OslpBootBlock *BootBlock,
    void *BootStack,
    void *PageMap,
    UINTN MemoryMapSize,
    EFI_MEMORY_DESCRIPTOR *MemoryMap,
    UINTN DescriptorSize,
    UINT32 DescriptorVersion);

#endif /* _PLATFORM_H_ */
