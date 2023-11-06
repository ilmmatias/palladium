/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <amd64/apic.h>
#include <amd64/halp.h>
#include <ke.h>
#include <mm.h>
#include <rt.h>
#include <string.h>
#include <vid.h>

typedef struct {
    char Stack[0x4000];
    int Online;
} HalpProcessor;

extern RtSList HalpLapicListHead;

extern void HalpApEntry(void);
extern uint64_t HalpKernelPageMap;
extern HalpProcessor *HalpApStructure;

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
    uint32_t ApicId = HalpGetCurrentApicId();
    RtSList *ListHeader = HalpLapicListHead.Next;

    /* Copy the AP startup code into 0x8000 (as the STARTUP IPI makes the API start at
       0800:0000). */
    memcpy(MI_PADDR_TO_VADDR(0x8000), HalpApEntry, MM_PAGE_SIZE);

    /* Save the kernel page map (shared between all processors).
       The address is guaranteed to be on the low 4GiBs (because of bootmgr). */
    __asm__ volatile("mov %%cr3, %0"
                     : "=r"(*(uint64_t *)MI_PADDR_TO_VADDR(
                         (uint64_t)&HalpKernelPageMap - (uint64_t)HalpApEntry + 0x8000)));

    while (ListHeader) {
        LapicEntry *Entry = CONTAINING_RECORD(ListHeader, LapicEntry, ListHeader);

        /* KiSystemStartup is already initializing the BSP. */
        if (Entry->ApicId == ApicId) {
            ListHeader = ListHeader->Next;
            continue;
        }

        HalpProcessor *Processor = MmAllocatePool(sizeof(HalpProcessor), "Halp");
        if (!Processor) {
            continue;
        }

        Processor->Online = 0;
        *(HalpProcessor **)MI_PADDR_TO_VADDR(
            (uint64_t)&HalpApStructure - (uint64_t)HalpApEntry + 0x8000) = Processor;

        /* Recommended/safe initialization process;
           Send an INIT IPI, followed by deasserting it. */
        HalpClearApicErrors();
        HalpSendIpi(Entry->ApicId, 0xC500);
        HalpWaitIpiDelivery();
        HalpSendIpi(Entry->ApicId, 0x8500);
        HalpWaitIpiDelivery();
        HalWaitTimer(10 * HAL_MILLISECS);

        /* Two attempts at sending a STARTUP IPI to the core; If it doesn't come alive, we're
           ignoring it. */
        for (int i = 0; i < 2; i++) {
            HalpClearApicErrors();
            HalpSendIpi(Entry->ApicId, 0x608);
            HalWaitTimer(200 * HAL_MICROSECS);
            HalpWaitIpiDelivery();
        }

        for (int i = 0; i < 100; i++) {
            if (Processor->Online) {
                VidPrint(KE_MESSAGE_DEBUG, "Kernel HAL", "AP came online!\n");
                break;
            }

            HalWaitTimer(10 * HAL_MILLISECS);
        }

        ListHeader = ListHeader->Next;
    }
}
