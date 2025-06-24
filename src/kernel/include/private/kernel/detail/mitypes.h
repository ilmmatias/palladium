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

typedef struct {
    RtDList ListHeader;
    union {
        struct {
            uint32_t Used : 1;
            uint32_t PoolItem : 1;
            uint32_t PoolBase : 1;
            uint32_t Padding : 29;
        };
        uint32_t Flags;
    };
    uint32_t Pages;
    char Tag[4];
} MiPageEntry;

typedef struct {
    RtSList ListHeader;
    char Tag[4];
    uint64_t Allocations;
    uint64_t AllocatedBytes;
    uint64_t MaxAllocations;
    uint64_t MaxAllocatedBytes;
    uint64_t Padding;
} MiPoolTrackerHeader;

#endif /* _KERNEL_DETAIL_MITYPES_H_ */
