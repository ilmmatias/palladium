/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_AMD64_KETYPES_H_
#define _KERNEL_DETAIL_AMD64_KETYPES_H_

/* We need to define these beforehand (because pstypes.h uses them). */
typedef uint64_t KeIrql;
typedef volatile uint64_t KeSpinLock;

struct PsThread;

#include <kernel/detail/amd64/haltypes.h>
#include <kernel/detail/kedefs.h>
#include <kernel/detail/mmdefs.h>
#include <kernel/detail/pstypes.h>

typedef struct {
    KeSpinLock Lock;
    uint32_t Number;
    uint32_t ApicId;
    RtDList KernelSignalQueue;
    RtAvlTree WaitTree;
    uint64_t ClosestWaitTick;
    RtDList ThreadQueue;
    RtDList TerminationQueue;
    struct PsThread *CurrentThread;
    struct PsThread *IdleThread;
    uint64_t ThreadCount;
    uint64_t Ticks;
    uint64_t HighIrqlTicks;
    uint64_t LowIrqlTicks;
    uint64_t IdleTicks;
    int EventType;
    RtDList FreePageListHead;
    uint64_t FreePageListSize;
    RtSList FreePoolPageListHead[4];
    uint64_t FreePoolPageListSize[4];
    RtSList FreePoolBlockListHead[MM_POOL_BLOCK_COUNT];
    uint64_t FreePoolBlockListSize[MM_POOL_BLOCK_COUNT];
    char *StackBase;
    char *StackLimit;
    char SystemStack[KE_STACK_SIZE] __attribute__((aligned(MM_PAGE_SIZE)));
    char NmiStack[KE_STACK_SIZE] __attribute__((aligned(MM_PAGE_SIZE)));
    char DoubleFaultStack[KE_STACK_SIZE] __attribute__((aligned(MM_PAGE_SIZE)));
    char MachineCheckStack[KE_STACK_SIZE] __attribute__((aligned(MM_PAGE_SIZE)));
    char GdtEntries[56];
    HalpTssEntry TssEntry;
    HalpIdtEntry IdtEntries[256];
    RtDList InterruptList[256];
    uint8_t InterruptUsage[256];
} KeProcessor;

#endif /* _KERNEL_DETAIL_AMD64_KETYPES_H_ */
