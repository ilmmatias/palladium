/* SPDX-FileCopyrightText: (C) 2024-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include <memory.h>
#include <stddef.h>

#if __has_include(ARCH_MAKE_INCLUDE_PATH_WITHOUT_PREFIX(platform.h))
#include ARCH_MAKE_INCLUDE_PATH_WITHOUT_PREFIX(platform.h)
#else
#error "Undefined ARCH for the CRT module!"
#endif /* __has_include */

#define OSLP_BOOT_MAGIC "OLDR"
#define OSLP_BOOT_VERSION 0x0000'0000'00000008

#define OSLP_DEBUG_TRANSPORT_NONE 0
#define OSLP_DEBUG_TRANSPORT_KDNET 1
#define OSLP_DEBUG_TRANSPORT_PC16550_PIO 2

#define OSLP_DEBUG_FLAG_ECHO_ENABLED (1u << 0)

#define OSLP_DEBUG_DISCONNECT_STOP 0
#define OSLP_DEBUG_DISCONNECT_CONTINUE 1

typedef struct __attribute__((packed)) {
    char Magic[4];
    uint64_t LoaderVersion;
    uint32_t BlockSize;
    RtDList *MemoryDescriptorListHead;
    RtDList *BootDriverListHead;
    uint64_t RandomSeed;
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

typedef struct __attribute__((packed)) {
    uint32_t Type;
    uint32_t Flags;
    uint32_t DisconnectPolicy;
    uint32_t DisconnectTimeoutMilliseconds;
    union {
        struct __attribute__((packed)) {
            uint8_t Address[4];
            uint16_t Port;
            uint16_t Reserved;
            uint32_t SegmentNumber;
            uint32_t BusNumber;
            uint32_t DeviceNumber;
            uint32_t FunctionNumber;
            void *Initializer;
        } KdNet;
        struct __attribute__((packed)) {
            uint64_t Address;
            uint32_t BaudRate;
            uint8_t Reserved[20];
        } Serial;
    };
} OslpBootDebugData;

typedef struct __attribute__((packed)) {
    OslpBootBasicData Basic;
    OslpBootAcpiData Acpi;
    OslpBootGraphicsData Graphics;
    OslpBootDebugData Debug;
    OslpBootArchData Arch;
} OslpBootBlock;

static_assert(sizeof(OslpBootBasicData) == 40);
static_assert(sizeof(OslpBootDebugData) == 48);
static_assert(offsetof(OslpBootBlock, Basic) == 0);
static_assert(offsetof(OslpBootBlock, Acpi) == 40);
static_assert(offsetof(OslpBootBlock, Graphics) == 64);
static_assert(offsetof(OslpBootBlock, Debug) == 92);
static_assert(offsetof(OslpBootBlock, Arch) == 140);
static_assert(sizeof(OslpBootBlock) == 148);

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
