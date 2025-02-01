/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_KETYPES_H_
#define _KERNEL_DETAIL_KETYPES_H_

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

#endif /* _KERNEL_DETAIL_KETYPES_H_ */
