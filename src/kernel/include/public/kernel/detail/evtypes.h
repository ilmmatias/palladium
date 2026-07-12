/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

/* IWYU pragma: private, include <kernel/ev.h> */

#ifndef _KERNEL_DETAIL_EVTYPES_H_
#define _KERNEL_DETAIL_EVTYPES_H_

#include <kernel/detail/evdefs.h>
#include <kernel/detail/ketypes.h>
#include <stddef.h>

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

static_assert(sizeof(EvHeader) == 40);
static_assert(offsetof(EvHeader, Type) == 0);
static_assert(offsetof(EvHeader, Lock) == 8);
static_assert(offsetof(EvHeader, WaitList) == 16);
static_assert(offsetof(EvHeader, Signaled) == 32);

typedef struct {
    EvHeader Header;
} EvSignal;

typedef struct {
    EvHeader Header;
    RtDList OwnerListHeader;
    uint64_t Recursion;
    uint64_t Contention;
    void *Owner;
} EvMutex;

#ifdef ARCH_amd64
static_assert(sizeof(EvMutex) == EV_AMD64_MUTEX_SIZE);
static_assert(offsetof(EvMutex, OwnerListHeader) == EV_AMD64_MUTEX_OWNER_LIST_OFFSET);
static_assert(offsetof(EvMutex, Recursion) == EV_AMD64_MUTEX_RECURSION_OFFSET);
static_assert(offsetof(EvMutex, Contention) == EV_AMD64_MUTEX_CONTENTION_OFFSET);
static_assert(offsetof(EvMutex, Owner) == EV_AMD64_MUTEX_OWNER_OFFSET);
#endif /* ARCH_amd64 */

#endif /* _KERNEL_DETAIL_EVTYPES_H_ */
