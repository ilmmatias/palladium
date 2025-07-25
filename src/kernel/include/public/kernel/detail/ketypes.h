/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_KETYPES_H_
#define _KERNEL_DETAIL_KETYPES_H_

#include <kernel/detail/kedefs.h>
#include <rt/avltree.h>
#include <rt/list.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, ketypes.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, ketypes.h)
#endif /* __has__include */
/* clang-format on */

typedef struct {
    RtDList ListHeader;
    void *ImageBase;
    void *EntryPoint;
    uint32_t SizeOfImage;
    const char *ImageName;
} KeModule;

typedef struct {
    RtDList ListHeader;
    void (*Routine)(void *);
    void *Context;
} KeWork;

typedef struct {
    uint64_t Size;
    volatile uint64_t Bits[KE_MAX_PROCESSORS / 64];
} KeAffinity;

#endif /* _KERNEL_DETAIL_KETYPES_H_ */
