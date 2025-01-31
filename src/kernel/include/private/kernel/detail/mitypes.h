/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_MITYPES_H_
#define _KERNEL_DETAIL_MITYPES_H_

#include <kernel/detail/mmtypes.h>
#include <rt/list.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, mitypes.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, mitypes.h)
#endif /* __has__include */
/* clang-format on */

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

#endif /* _KERNEL_DETAIL_MITYPES_H_ */
