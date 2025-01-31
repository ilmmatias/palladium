/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_PSTYPES_H_
#define _KERNEL_DETAIL_PSTYPES_H_

#include <kernel/detail/haltypes.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, pstypes.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, pstypes.h)
#endif /* __has__include */
/* clang-format on */

typedef struct PsThread {
    RtDList ListHeader;
    uint64_t ExpirationTicks;
    uint64_t WaitTicks;
    char *Stack;
    char *StackLimit;
    HalContextFrame ContextFrame;
} PsThread;

#endif /* _KERNEL_DETAIL_PSTYPES_H_ */
