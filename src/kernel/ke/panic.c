/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
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
    /* We don't care about the current IRQL, reset it to DISPATCH, or most functions we want to use
     * won't work. */
    HalpEnterCriticalSection();
    HalpSetIrql(KE_IRQL_DISPATCH);

    /* Panics always halt everyone (the system isn't in a safe state anymore). */
    for (RtSList *ListHeader = HalpProcessorListHead.Next; ListHeader;
         ListHeader = ListHeader->Next) {
        HalProcessor *Processor = CONTAINING_RECORD(ListHeader, HalProcessor, ListHeader);
        Processor->EventStatus = HAL_PANIC_EVENT;
        HalpNotifyProcessor(Processor, 0);
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
