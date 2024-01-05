/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <amd64/apic.h>
#include <amd64/halp.h>
#include <amd64/msr.h>
#include <mm.h>
#include <string.h>

extern RtSList HalpLapicListHead;

extern void HalpApEntry(void);
extern uint64_t HalpKernelPageMap;
extern HalpProcessor *HalpApStructure;

RtSList HalpProcessorListHead = {};

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function switches on all APs (Application Processors, aka, CPUs other than the startup
 *     CPU), and starts their setup process.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpInitializeSmp(void) {
    uint32_t ApicId = HalpReadLapicRegister(0x20);
    RtSList *ListHeader = HalpLapicListHead.Next;

    /* Copy the AP startup code into 0x8000 (as the STARTUP IPI makes the API start at
       0800:0000). */
    memcpy(MI_PADDR_TO_VADDR(0x8000), HalpApEntry, MM_PAGE_SIZE);

    /* Save the kernel page map (shared between all processors).
       The address is guaranteed to be on the low 4GiBs (because of bootmgr). */
    __asm__ volatile("mov %%cr3, %0"
                     : "=r"(*(uint64_t *)MI_PADDR_TO_VADDR(
                         (uint64_t)&HalpKernelPageMap - (uint64_t)HalpApEntry + 0x8000)));

    /* BSP processor is already initialized, just setup its processor struct before someone
       tries acessing our scheduler. */
    HalpProcessor *Processor = (HalpProcessor *)HalGetCurrentProcessor();

    Processor->Base.Online = 1;
    Processor->ApicId = ApicId;

    Processor->Base.ThreadQueueSize = 1;
    Processor->Base.ThreadQueueLock = 0;
    RtInitializeDList(&Processor->Base.ThreadQueue);

    Processor->Base.ForceYield = 0;
    Processor->Base.ClosestEvent = 0;
    RtInitializeDList(&Processor->Base.DpcQueue);
    RtInitializeDList(&Processor->Base.EventQueue);

    RtPushSList(&HalpProcessorListHead, &Processor->Base.ListHeader);

    while (ListHeader) {
        LapicEntry *Entry = CONTAINING_RECORD(ListHeader, LapicEntry, ListHeader);

        /* KiSystemStartup is already initializing the BSP. */
        if (Entry->ApicId == ApicId) {
            ListHeader = ListHeader->Next;
            continue;
        }

        Processor = MmAllocatePool(sizeof(HalpProcessor), "Halp");
        if (!Processor) {
            continue;
        }

        Processor->Base.Online = 0;
        Processor->ApicId = Entry->ApicId;

        /* Initialize the scheduler queue before any other processor has any chances to access
           us. */
        Processor->Base.ThreadQueueSize = 0;
        Processor->Base.ThreadQueueLock = 0;
        RtInitializeDList(&Processor->Base.ThreadQueue);

        Processor->Base.ForceYield = 0;
        Processor->Base.ClosestEvent = 0;
        RtInitializeDList(&Processor->Base.DpcQueue);
        RtInitializeDList(&Processor->Base.EventQueue);

        *(HalpProcessor **)MI_PADDR_TO_VADDR(
            (uint64_t)&HalpApStructure - (uint64_t)HalpApEntry + 0x8000) = Processor;

        /* Recommended/safe initialization process;
           Send an INIT IPI, followed by deasserting it. */
        HalpClearApicErrors();
        HalpSendIpi(Entry->ApicId, 0xC500);
        HalpWaitIpiDelivery();
        HalpSendIpi(Entry->ApicId, 0x8500);
        HalpWaitIpiDelivery();
        HalWaitTimer(10 * EV_MILLISECS);

        /* Two attempts at sending a STARTUP IPI should be enough (according to spec). */
        for (int i = 0; i < 2; i++) {
            HalpClearApicErrors();
            HalpSendIpi(Entry->ApicId, 0x608);
            HalWaitTimer(200 * EV_MICROSECS);
            HalpWaitIpiDelivery();
        }

        while (!__atomic_load_n(&Processor->Base.Online, __ATOMIC_RELAXED)) {
            HalWaitTimer(200 * EV_MICROSECS);
        }

        RtPushSList(&HalpProcessorListHead, &Processor->Base.ListHeader);
        ListHeader = ListHeader->Next;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function gets a pointer to the processor-specific structure of the current processor.
 *     This only works after HalpInitializePlatform().
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Pointer to the processor struct.
 *-----------------------------------------------------------------------------------------------*/
HalProcessor *HalGetCurrentProcessor(void) {
    return (HalProcessor *)ReadMsr(0xC0000102);
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
void HalpNotifyProcessor(HalProcessor *Processor, int WaitDelivery) {
    HalpSendIpi(((HalpProcessor *)Processor)->ApicId, 0x40);

    if (WaitDelivery) {
        HalpWaitIpiDelivery();
    }
}
