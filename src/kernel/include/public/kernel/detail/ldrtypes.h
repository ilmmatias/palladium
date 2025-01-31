/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_LDRTYPES_H_
#define _KERNEL_DETAIL_LDRTYPES_H_

#include <rt/list.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, ldrtypes.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, ldrtypes.h)
#endif /* __has__include */
/* clang-format on */

typedef struct {
    RtSList ListHeader;
    const char *Name;
    uint64_t ImageBase;
    uint64_t ImageSize;
} LdrModule;

#endif /* _KERNEL_DETAIL_LDRTYPES_H_ */
