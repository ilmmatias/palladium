/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp.h>
#include <ki.h>
#include <mm.h>
#include <rt/except.h>
#include <vid.h>

static char *Messages[] = {
    "An unspecified (but fatal) error occurred.\n",
    ("Your computer does not have compliant ACPI tables.\n"
     "Check with your system's manufacturer for a BIOS update.\n"),
    "The kernel pool allocator data has been corrupted.\n",
    "The kernel or a driver tried freeing the same pool data twice.\n",
    "The kernel or a driver tried freeing the same page twice.\n",
    "No memory left for an unpagable kernel allocation.\n",
    "The kernel or a driver tried using a function in the wrong context.\n",
};

KeSpinLock KiPanicLock = {0};
uint32_t KiPanicLockedProcessors = 0;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function takes over the display, and writes a fatal error message (panic), followed by
 *     halting the system.
 *
 * PARAMETERS:
 *     Message - Number of the error code/message to be shown.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void KeFatalError(int Message) {
    KeProcessor *Processor = (KeProcessor *)HalGetCurrentProcessor();

    /* We don't care about the current IRQL, reset it to DISPATCH, or most functions we want to use
     * won't work. */
    HalpEnterCriticalSection();
    HalpSetIrql(KE_IRQL_DISPATCH);

    /* This should just halt if someone else already panicked; No need for LockGuard, we're
     * not releasing this. */
    __atomic_add_fetch(&KiPanicLockedProcessors, 1, __ATOMIC_SEQ_CST);
    KeAcquireSpinLock(&KiPanicLock);

    /* Panics always halt everyone (the system isn't in a safe state anymore). */
    for (uint32_t i = 0; i < HalpProcessorCount; i++) {
        if (HalpProcessorList[i] != Processor) {
            HalpProcessorList[i]->EventStatus = KE_PANIC_EVENT;
            HalpNotifyProcessor(HalpProcessorList[i], 0);
        }
    }

    /* Wait until everyone is halted; We don't want any processor doing anything if we crashed. */
    for (int i = 0; i < 10; i++) {
        if (__atomic_load_n(&KiPanicLockedProcessors, __ATOMIC_RELAXED) == HalpProcessorCount) {
            break;
        }

        HalWaitTimer(100 * EV_MILLISECS);
    }

    if (Message < KE_FATAL_ERROR || Message >= KE_PANIC_COUNT) {
        Message = KE_FATAL_ERROR;
    }

    VidSetColor(VID_COLOR_PANIC);
    VidPutString("CANNOT SAFELY RECOVER OPERATION\n");
    VidPutString(Messages[Message]);

    RtContext Context;
    RtSaveContext(&Context);
    VidPutString("\nSTACK TRACE:\n");

    while (1) {
        KiDumpSymbol((void *)Context.Rip);

        if (Context.Rip < MM_PAGE_SIZE) {
            break;
        }

        uint64_t ImageBase = RtLookupImageBase(Context.Rip);
        RtVirtualUnwind(
            RT_UNW_FLAG_NHANDLER,
            ImageBase,
            Context.Rip,
            RtLookupFunctionEntry(ImageBase, Context.Rip),
            &Context,
            NULL,
            NULL);
    }

    while (1) {
        HalpStopProcessor();
    }
}
