/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_OBPTYPES_H_
#define _KERNEL_DETAIL_OBPTYPES_H_

#include <kernel/detail/obtypes.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, obptypes.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, obptypes.h)
#endif /* __has__include */
/* clang-format on */

typedef struct {
    RtDList HashHeader;
    char *Name;
    void *Object;
    ObDirectory *Parent;
} ObpDirectoryEntry;

typedef struct {
    ObType *Type;
    ObpDirectoryEntry *Parent;
    uint32_t References;
    char Tag[4];
} ObpObject;

#endif /* _KERNEL_DETAIL_OBPTYPES_H_ */
