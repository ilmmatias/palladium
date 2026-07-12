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
    bool Enabled;
    bool EchoEnabled;
    uint8_t Address[4];
    uint16_t Port;
    uint32_t SegmentNumber;
    uint32_t BusNumber;
    uint32_t DeviceNumber;
    uint32_t FunctionNumber;
    void *Initializer;
} KiLoaderDebugData;

typedef struct __attribute__((packed)) {
    uint32_t Type;
    uint32_t BaudRate;
    uint64_t Address;
} KiLoaderDiagnosticData;

typedef struct __attribute__((packed)) {
    KiLoaderBasicData Basic;
    KiLoaderAcpiData Acpi;
    KiLoaderGraphicsData Graphics;
    KiLoaderDebugData Debug;
    KiLoaderDiagnosticData Diagnostic;
    KiLoaderArchData Arch;
} KiLoaderBlock;

static_assert(sizeof(KiLoaderBasicData) == 40);
static_assert(sizeof(KiLoaderDiagnosticData) == 16);
static_assert(offsetof(KiLoaderBlock, Basic) == 0);
static_assert(offsetof(KiLoaderBlock, Acpi) == 40);
static_assert(offsetof(KiLoaderBlock, Graphics) == 64);
static_assert(offsetof(KiLoaderBlock, Debug) == 92);
static_assert(offsetof(KiLoaderBlock, Diagnostic) == 124);
static_assert(offsetof(KiLoaderBlock, Arch) == 140);
static_assert(sizeof(KiLoaderBlock) == 148);

#endif /* _KERNEL_DETAIL_KITYPES_H_ */
