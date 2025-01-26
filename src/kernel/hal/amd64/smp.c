/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <amd64/apic.h>
#include <amd64/halp.h>
#include <amd64/msr.h>
#include <ke.h>
#include <mi.h>
#include <string.h>
#include <vid.h>

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
    uint32_t ApicId = HalGetCurrentProcessor()->ApicId;

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
        VidPrint(
            VID_MESSAGE_ERROR, "Kernel HAL", "couldn't allocate space for the processor list\n");
        KeFatalError(KE_PANIC_INSTALL_MORE_MEMORY);
    }

    for (uint32_t i = 0; i < HalpProcessorCount; i++) {
        if (!i) {
            HalpProcessorList[i] = HalGetCurrentProcessor();
        } else {
            HalpProcessorList[i] = MmAllocatePool(sizeof(KeProcessor), "Halp");
        }

        if (!HalpProcessorList[i]) {
            VidPrint(
                VID_MESSAGE_ERROR, "Kernel HAL", "couldn't allocate space for the a processor\n");
            KeFatalError(KE_PANIC_INSTALL_MORE_MEMORY);
        }

        RtInitializeDList(&HalpProcessorList[i]->ThreadQueue);
        RtInitializeDList(&HalpProcessorList[i]->DpcQueue);
        RtInitializeDList(&HalpProcessorList[i]->EventQueue);
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
 *     This function gets a pointer to the processor-specific structure of the current processor.
 *     The result of this will always be NULL before HalpInitializePlatform().
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Pointer to the processor struct.
 *-----------------------------------------------------------------------------------------------*/
KeProcessor *HalGetCurrentProcessor(void) {
    return (KeProcessor *)ReadMsr(0xC0000102);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function notifies another processor that some (probably significant) event has
 *     happend.
 *
 * PARAMETERS:
 *     Processor - Which processor to notify.
 *     WaitDelivery - Set this to 1 if we should wait for delivery (important for events!)
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpNotifyProcessor(KeProcessor *Processor, int WaitDelivery) {
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
    Processor->EventStatus = KE_EVENT_FREEZE;
    HalpSendNmi(Processor->ApicId);
}
