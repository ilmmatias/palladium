/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

/* IWYU pragma: private, include <kernel/ki.h> */

#ifndef _KERNEL_DETAIL_KITYPES_H_
#define _KERNEL_DETAIL_KITYPES_H_

#include <kernel/detail/ketypes.h>
#include <stddef.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, kitypes.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, kitypes.h)
#endif /* __has__include */
/* clang-format on */

typedef struct __attribute__((packed)) {
    char Magic[4];
    uint64_t LoaderVersion;
    uint32_t BlockSize;
    RtDList *MemoryDescriptorListHead;
    RtDList *BootDriverListHead;
    uint64_t RandomSeed;
} KiLoaderBasicData;

typedef struct __attribute__((packed)) {
    uint32_t Version;
    void *RootPointer;
    void *RootTable;
    uint32_t RootTableSize;
} KiLoaderAcpiData;

typedef struct __attribute__((packed)) {
    void *BackBuffer;
    void *FrontBuffer;
    uint32_t Width;
    uint32_t Height;
    uint32_t Pitch;
} KiLoaderGraphicsData;

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
} KiLoaderDebugData;

typedef struct __attribute__((packed)) {
    KiLoaderBasicData Basic;
    KiLoaderAcpiData Acpi;
    KiLoaderGraphicsData Graphics;
    KiLoaderDebugData Debug;
    KiLoaderArchData Arch;
} KiLoaderBlock;

static_assert(sizeof(KiLoaderBasicData) == 40);
static_assert(sizeof(KiLoaderDebugData) == 48);
static_assert(offsetof(KiLoaderBlock, Basic) == 0);
static_assert(offsetof(KiLoaderBlock, Acpi) == 40);
static_assert(offsetof(KiLoaderBlock, Graphics) == 64);
static_assert(offsetof(KiLoaderBlock, Debug) == 92);
static_assert(offsetof(KiLoaderBlock, Arch) == 140);
static_assert(sizeof(KiLoaderBlock) == 148);

#endif /* _KERNEL_DETAIL_KITYPES_H_ */
