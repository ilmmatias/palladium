/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

/* IWYU pragma: private, include <kernel/ps.h> */

#ifndef _KERNEL_DETAIL_PSTYPES_H_
#define _KERNEL_DETAIL_PSTYPES_H_

#include <kernel/detail/haltypes.h>
#include <stddef.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, pstypes.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, pstypes.h)
#endif /* __has__include */
/* clang-format on */

typedef struct PsThread {
    /* The struct needs to be inlined like this, as we can't include evtypes.h over here; Keep this
     * synched with EvHeader! */
    struct {
        uint8_t Type;
        KeSpinLock Lock;
        RtDList WaitList;
        bool Signaled;
    } EventHeader;
    RtDList ListHeader;
    RtDList OwnedMutexList;
    RtDList WaitListHeader;
    RtAvlNode WaitTreeNode;
    KeSpinLock AlertLock;
    RtSList AlertList;
    bool AlertListBlocked;
    uint8_t State;
    uint64_t ExpirationTicks;
    uint64_t WaitTicks;
    uint64_t WaitGeneration;
    void *WaitObject;
    uint8_t WaitCompletion;
    bool WaitListLinked;
    bool WaitTreeLinked;
    void *Processor;
    char *Stack;
    char *StackLimit;
    char *AllocatedStack;
    HalContextFrame ContextFrame;
} PsThread;

#ifdef ARCH_amd64
static_assert(sizeof(PsThread) == PS_AMD64_THREAD_SIZE);
static_assert(offsetof(PsThread, OwnedMutexList) == PS_AMD64_THREAD_OWNED_MUTEX_LIST_OFFSET);
static_assert(offsetof(PsThread, WaitGeneration) == PS_AMD64_THREAD_WAIT_GENERATION_OFFSET);
static_assert(offsetof(PsThread, WaitObject) == PS_AMD64_THREAD_WAIT_OBJECT_OFFSET);
static_assert(offsetof(PsThread, WaitCompletion) == PS_AMD64_THREAD_WAIT_COMPLETION_OFFSET);
static_assert(offsetof(PsThread, Processor) == PS_AMD64_THREAD_PROCESSOR_OFFSET);
static_assert(offsetof(PsThread, ContextFrame) == PS_AMD64_THREAD_CONTEXT_OFFSET);
#endif /* ARCH_amd64 */

typedef struct {
    RtSList ListHeader;
    void (*Routine)(void *);
    void *Context;
    bool Queued;
    bool PoolAllocated;
} PsAlert;

#endif /* _KERNEL_DETAIL_PSTYPES_H_ */
