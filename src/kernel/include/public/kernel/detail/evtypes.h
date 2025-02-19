/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_EVTYPES_H_
#define _KERNEL_DETAIL_EVTYPES_H_

#include <kernel/detail/evdefs.h>
#include <kernel/detail/ketypes.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, evtypes.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, evtypes.h)
#endif /* __has__include */
/* clang-format on */

typedef struct {
    uint8_t Type;
    KeSpinLock Lock;
    RtDList WaitList;
    bool Signaled;
} EvHeader;

typedef struct {
    EvHeader Header;
} EvSignal;

typedef struct {
    EvHeader Header;
    uint64_t Recursion;
    uint64_t Contention;
    void *Owner;
} EvMutex;

#endif /* _KERNEL_DETAIL_EVTYPES_H_ */
