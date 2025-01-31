/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_HALTYPES_H_
#define _KERNEL_DETAIL_HALTYPES_H_

#include <kernel/detail/ketypes.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, haltypes.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, haltypes.h)
#endif /* __has__include */
/* clang-format on */

typedef struct HalInterrupt {
    RtDList ListHeader;
    bool Enabled;
    KeSpinLock Lock;
    KeIrql Irql;
    uint32_t Vector;
    uint8_t Type;
    void (*Handler)(HalInterruptFrame *, void *);
    void *HandlerContext;
} HalInterrupt;

#endif /* _KERNEL_DETAIL_HALTYPES_H_ */
