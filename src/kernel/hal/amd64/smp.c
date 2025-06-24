/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/ev.h>
#include <kernel/halp.h>
#include <kernel/intrin.h>
#include <kernel/ke.h>
#include <kernel/mm.h>
#include <string.h>

extern void HalpApplicationProcessorEntry(void);
extern uint64_t HalpKernelPageMap;
extern RtSList HalpLapicListHead;

bool HalpSmpInitializationComplete = false;
KeProcessor **HalpProcessorList = NULL;
uint32_t HalpOnlineProcessorCount = 0;
uint32_t HalpProcessorCount = 0;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function gets all APs (Application Processors, aka, CPUs other than the startup CPU)
 *     online, and starts their setup process.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpInitializeSmp(void) {
    uint32_t ApicId = KeGetCurrentProcessor()->ApicId;

    /* Map the AP startup code (0x8000), and copy all the trampoline data to it. */
    uint64_t EntryAddress = 0x8000;
    HalpMapContiguousPages(
        (void *)EntryAddress, EntryAddress, MM_PAGE_SIZE, MI_MAP_WRITE | MI_MAP_EXEC);
    memcpy((void *)EntryAddress, HalpApplicationProcessorEntry, MM_PAGE_SIZE);

    /* Save the kernel page map (shared between all processors).
       The address is guaranteed to be on the low 4GiBs (because of bootmgr). */
    __asm__ volatile("mov %%cr3, %0"
                     : "=r"(*(uint64_t *)((uint64_t)&HalpKernelPageMap -
                                          (uint64_t)HalpApplicationProcessorEntry + EntryAddress)));

    /* Allocate space for all the processors (this is just the pointer list, we'll be allocating
     * each processor after that). */
    HalpProcessorList =
        MmAllocatePool(HalpProcessorCount * sizeof(KeProcessor *), MM_POOL_TAG_PROCESSOR);
    if (!HalpProcessorList) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_SMP_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
            0,
            0);
    }

    for (uint32_t i = 0; i < HalpProcessorCount; i++) {
        if (!i) {
            HalpProcessorList[i] = KeGetCurrentProcessor();
        } else {
            HalpProcessorList[i] = MmAllocatePool(sizeof(KeProcessor), MM_POOL_TAG_PROCESSOR);
        }

        if (!HalpProcessorList[i]) {
            KeFatalError(
                KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_SMP_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
                0,
                0);
        }

        HalpProcessorList[i]->Number = i;

        RtInitializeDList(&HalpProcessorList[i]->DpcQueue);
        RtInitializeDList(&HalpProcessorList[i]->WaitQueue);
        RtInitializeDList(&HalpProcessorList[i]->ThreadQueue);
        RtInitializeDList(&HalpProcessorList[i]->TerminationQueue);

        /* There is no need to initialize the memory manager caches for the BSP; Actually, doing
         * that would break things (because it's already in use!) */
        if (i) {
            RtInitializeDList(&HalpProcessorList[i]->FreePageListHead);
        }
    }

    /* Now, we can start waking up everyone. */
    HalpOnlineProcessorCount = 1;

    RtSList *ListHeader = HalpLapicListHead.Next;
    while (ListHeader) {
        HalpLapicEntry *Entry = CONTAINING_RECORD(ListHeader, HalpLapicEntry, ListHeader);
        if (Entry->ApicId == ApicId) {
            ListHeader = ListHeader->Next;
            continue;
        }

        /* Recommended/safe initialization process;
         * Send an INIT IPI, followed by deasserting it. */
        HalpSendIpi(Entry->ApicId, 0, HALP_APIC_ICR_DELIVERY_INIT);
        HalWaitTimer(10 * EV_MICROSECS);
        HalpSendIpi(Entry->ApicId, 0, HALP_APIC_ICR_DELIVERY_INIT_DEASSERT);
        HalWaitTimer(200 * EV_MICROSECS);

        /* Two attempts at sending a STARTUP IPI should be enough (according to spec). */
        HalpSendIpi(Entry->ApicId, EntryAddress >> 12, HALP_APIC_ICR_DELIVERY_STARTUP);
        HalpSendIpi(Entry->ApicId, EntryAddress >> 12, HALP_APIC_ICR_DELIVERY_STARTUP);

        ListHeader = ListHeader->Next;
    }

    while (__atomic_load_n(&HalpOnlineProcessorCount, __ATOMIC_RELAXED) != HalpProcessorCount) {
        HalWaitTimer(100 * EV_MICROSECS);
    }

    HalpUnmapPages((void *)EntryAddress, MM_PAGE_SIZE);
    HalpSmpInitializationComplete = true;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function notifies all but the current processor that the IPI handler should run.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpBroadcastIpi(void) {
    KeProcessor *CurrentProcessor = KeGetCurrentProcessor();
    for (uint32_t i = 0; i < HalpOnlineProcessorCount; i++) {
        if (i != CurrentProcessor->Number) {
            HalpSendIpi(
                HalpProcessorList[i]->ApicId, HALP_INT_IPI_VECTOR, HALP_APIC_ICR_DELIVERY_FIXED);
        }
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function notifies all but the current processor that they should stop running.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpBroadcastFreeze(void) {
    KeProcessor *CurrentProcessor = KeGetCurrentProcessor();
    for (uint32_t i = 0; i < HalpOnlineProcessorCount; i++) {
        if (i != CurrentProcessor->Number) {
            HalpProcessorList[i]->EventType = KE_EVENT_TYPE_FREEZE;
            HalpSendIpi(HalpProcessorList[i]->ApicId, 0, HALP_APIC_ICR_DELIVERY_NMI);
        }
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function notifies another processor that a dispatch level event has happend.
 *
 * PARAMETERS:
 *     Processor - Which processor to notify.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpNotifyProcessor(KeProcessor *Processor) {
    HalpSendIpi(Processor->ApicId, HALP_INT_DISPATCH_VECTOR, HALP_APIC_ICR_DELIVERY_FIXED);
}
