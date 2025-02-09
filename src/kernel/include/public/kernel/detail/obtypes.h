/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_OBTYPES_H_
#define _KERNEL_DETAIL_OBTYPES_H_

#include <stdint.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, obtypes.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, obtypes.h)
#endif /* __has__include */
/* clang-format on */

typedef struct {
    const char Name[8];
    uint64_t Size;
    void (*Delete)(void *);
} ObTypeHeader;

#endif /* _KERNEL_DETAIL_OBTYPES_H_ */
