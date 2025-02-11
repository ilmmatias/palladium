/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_OBTYPES_H_
#define _KERNEL_DETAIL_OBTYPES_H_

#include <kernel/detail/ketypes.h>
#include <kernel/detail/obdefs.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, obtypes.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, obtypes.h)
#endif /* __has__include */
/* clang-format on */

typedef struct {
    const char *Name;
    uint64_t Size;
    void (*Delete)(void *);
} ObType;

typedef struct {
    RtDList HashHeads[32];
    KeSpinLock Lock;
} ObDirectory;

#endif /* _KERNEL_DETAIL_OBTYPES_H_ */
