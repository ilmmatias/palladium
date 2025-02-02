/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/detail/amd64/apic.h>
#include <kernel/ev.h>
#include <kernel/halp.h>
#include <kernel/intrin.h>
#include <kernel/ke.h>
#include <kernel/mi.h>
#include <string.h>

extern void HalpApplicationProcessorEntry(void);
extern uint64_t HalpKernelPageMap;
extern RtSList HalpLapicListHead;

KeProcessor **HalpProcessorList = NULL;
uint32_t HalpOnlineProcessorCount = 1;

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
    HalpMapPage((void *)0x8000, 0x8000, MI_MAP_WRITE | MI_MAP_EXEC);
    memcpy((void *)0x8000, HalpApplicationProcessorEntry, MM_PAGE_SIZE);

    /* Save the kernel page map (shared between all processors).
       The address is guaranteed to be on the low 4GiBs (because of bootmgr). */
    __asm__ volatile("mov %%cr3, %0"
                     : "=r"(*(uint64_t *)((uint64_t)&HalpKernelPageMap -
                                          (uint64_t)HalpApplicationProcessorEntry + 0x8000)));

    /* Allocate space for all the processors (this is just the pointer list, we'll be allocating
     * each processor after that). */
    HalpProcessorList = MmAllocatePool(HalpProcessorCount * sizeof(KeProcessor *), "Halp");
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
            HalpProcessorList[i] = MmAllocatePool(sizeof(KeProcessor), "Halp");
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

        RtInitializeDList(&HalpProcessorList[i]->WaitQueue);
        RtInitializeDList(&HalpProcessorList[i]->ThreadQueue);
        RtInitializeDList(&HalpProcessorList[i]->TerminationQueue);
    }

    RtSList *ListHeader = HalpLapicListHead.Next;
    while (ListHeader) {
        LapicEntry *Entry = CONTAINING_RECORD(ListHeader, LapicEntry, ListHeader);
        if (Entry->ApicId == ApicId) {
            ListHeader = ListHeader->Next;
            continue;
        }

        /* Recommended/safe initialization process;
         * Send an INIT IPI, followed by deasserting it. */
        HalpSendIpi(Entry->ApicId, 0xC500);
        HalpWaitIpiDelivery();
        HalWaitTimer(10 * EV_MICROSECS);
        HalpSendIpi(Entry->ApicId, 0x8500);
        HalpWaitIpiDelivery();
        HalWaitTimer(200 * EV_MICROSECS);

        /* Two attempts at sending a STARTUP IPI should be enough (according to spec). */
        for (int i = 0; i < 2; i++) {
            HalpSendIpi(Entry->ApicId, 0x608);
            HalWaitTimer(200 * EV_MICROSECS);
            HalpWaitIpiDelivery();
        }

        ListHeader = ListHeader->Next;
    }

    while (__atomic_load_n(&HalpOnlineProcessorCount, __ATOMIC_RELAXED) != HalpProcessorCount) {
        HalWaitTimer(100 * EV_MICROSECS);
    }

    HalpUnmapPage((void *)0x8000);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function notifies another processor that some (probably significant) event has
 *     happend.
 *
 * PARAMETERS:
 *     Processor - Which processor to notify.
 *     WaitDelivery - Set this to true if we should wait for delivery (important for events!)
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpNotifyProcessor(KeProcessor *Processor, bool WaitDelivery) {
    HalpSendIpi(Processor->ApicId, HAL_INT_DISPATCH_VECTOR);

    if (WaitDelivery) {
        HalpWaitIpiDelivery();
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function notifies another processor that it should stop running (permanently, as we
 *     don't have a way to specify it should start running again for now).
 *
 * PARAMETERS:
 *     Processor - Which processor to notify.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpFreezeProcessor(KeProcessor *Processor) {
    Processor->EventType = KE_EVENT_TYPE_FREEZE;
    HalpSendNmi(Processor->ApicId);
}
