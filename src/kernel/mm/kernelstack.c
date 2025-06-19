/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/ke.h>
#include <kernel/mi.h>
#include <string.h>

static KeSpinLock Lock = {0};
static RtSList FreeListHead = {};
static uint64_t FreeListSize = 0;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function allocates a new clean kernel stack.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Either a pointer to the stack start, or NULL on allocation failure.
 *-----------------------------------------------------------------------------------------------*/
void* MmAllocateKernelStack(void) {
    KeIrql OldIrql = KeRaiseIrql(KE_IRQL_DISPATCH);
    KeProcessor* Processor = KeGetCurrentProcessor();

    /* Trigger a cache refill if it's the first allocation we're doing (or if we dropped below the
     * lower limit). */
    if (Processor->FreeKernelStackListSize < MI_PROCESSOR_KERNEL_STACK_CACHE_LOW_LIMIT) {
        for (int i = 0; i < MI_PROCESSOR_KERNEL_STACK_CACHE_BATCH_SIZE; i++) {
            KeAcquireSpinLockAtCurrentIrql(&Lock);

            RtSList* ListHeader = RtPopSList(&FreeListHead);

            /* Attempt allocating a new fully new kernel stack if the global cache is also empty. */
            if (!ListHeader) {
                KeReleaseSpinLockAtCurrentIrql(&Lock);
                ListHeader = MmAllocatePool(KE_STACK_SIZE, MM_POOL_TAG_KERNEL_STACK);
                if (!ListHeader) {
                    break;
                }
            } else {
                FreeListSize--;
                KeReleaseSpinLockAtCurrentIrql(&Lock);
            }

            RtPushSList(&Processor->FreeKernelStackListHead, ListHeader);
            Processor->FreeKernelStackListSize++;
        }
    }

    /* Now we should just be able to pop from the local cache (if that fails, the system is out of
     * memory). */
    RtSList* ListHeader = RtPopSList(&Processor->FreeKernelStackListHead);
    KeLowerIrql(OldIrql);
    if (!ListHeader) {
        return NULL;
    }

    /* Each kernel stack is essentially an union, where the first field is a ListHeader if the stack
     * is free, so we can just directly return (no need to use CONTAINING_RECORD). */
    Processor->FreeKernelStackListSize--;
    memset(ListHeader, 0, KE_STACK_SIZE);
    return ListHeader;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function frees a previously allocated kernel stack.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Either a pointer to the stack start, or NULL on allocation failure.
 *-----------------------------------------------------------------------------------------------*/
void MmFreeKernelStack(void* Base) {
    KeIrql OldIrql = KeRaiseIrql(KE_IRQL_DISPATCH);
    KeProcessor* Processor = KeGetCurrentProcessor();

    /* Check if we can just append this to the local cache. */
    if (Processor->FreeKernelStackListSize < MI_PROCESSOR_KERNEL_STACK_CACHE_HIGH_LIMIT) {
        RtPushSList(&Processor->FreeKernelStackListHead, Base);
        Processor->FreeKernelStackListSize++;
        KeLowerIrql(OldIrql);
        return;
    }

    /* Otherwise, let's first check if we're not above the hard limit; If so, we're probably under
     * VERY HIGH thread creation pressure; Keep the cache, and just free the given entry directly
     * back to the big pool.  */
    if (FreeListSize >= MI_GLOBAL_KERNEL_STACK_CACHE_HARD_LIMIT) {
        KeLowerIrql(OldIrql);
        MmFreePool(Base, MM_POOL_TAG_KERNEL_STACK);
        return;
    }

    /* If we're below it, just remove some entries out of the local list (and return the given
     * allocation to the global list rather than the local list as well). */
    RtSList ListHead = {};
    for (int i = 0; i < MI_PROCESSOR_KERNEL_STACK_CACHE_BATCH_SIZE; i++) {
        RtPushSList(&ListHead, RtPopSList(&Processor->FreeKernelStackListHead));
    }

    /* Push the newly freed item stack itself, and combine the global and temporary lists while
     * holding the lock. */
    RtPushSList(&ListHead, Base);
    KeAcquireSpinLockAtCurrentIrql(&Lock);
    RtSpliceSList(&FreeListHead, &ListHead);
    FreeListSize += MI_PROCESSOR_KERNEL_STACK_CACHE_BATCH_SIZE + 1;
    Processor->FreeKernelStackListSize -= MI_PROCESSOR_KERNEL_STACK_CACHE_BATCH_SIZE;
    KeReleaseSpinLockAndLowerIrql(&Lock, OldIrql);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function runs before a processor is about to enter idle/low power mode, and tries
 *     returning some globally cached kernel stacks to the big pool if we're above the soft limit.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void MiTryReturnKernelStacks(void) {
    /* Don't bother if we seem to be below the soft limit. */
    if (FreeListSize < MI_GLOBAL_KERNEL_STACK_CACHE_SOFT_LIMIT) {
        return;
    }

    /* Don't bother if we can't acquire the lock first try. */
    KeIrql OldIrql = KeRaiseIrql(KE_IRQL_DISPATCH);
    if (!KeTryAcquireSpinLockAtCurrentIrql(&Lock)) {
        KeLowerIrql(OldIrql);
        return;
    }

    /* We'll always be returning half of everything in the global list if we hit the soft limit
     * (that should be enough to not have too much hoarded, even more so if we actually hit the
     * higher limit rather than the lower). */
    RtSList ListHead = {};
    int Count = FreeListSize / 2;
    for (int i = 0; i < Count; i++) {
        RtPushSList(&ListHead, RtPopSList(&FreeListHead));
    }

    FreeListSize -= Count;
    KeReleaseSpinLockAndLowerIrql(&Lock, OldIrql);

    /* Now, outside the lock, we can free up all the memory we collected. */
    while (ListHead.Next) {
        MmFreePool(RtPopSList(&ListHead), MM_POOL_TAG_KERNEL_STACK);
    }
}
