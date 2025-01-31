/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_KITYPES_H_
#define _KERNEL_DETAIL_KITYPES_H_

#include <kernel/detail/ketypes.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, kitypes.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, kitypes.h)
#endif /* __has__include */
/* clang-format on */

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
} KiLoaderBlock;

#endif /* _KERNEL_DETAIL_KITYPES_H_ */
