/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <hal.h>
#include <ki.h>
#include <mm.h>
#include <rt/except.h>
#include <vid.h>

static char *Messages[] = {
    "An unspecified (but fatal) error occurred.\n",
    "Your computer does not have compliant ACPI tables.\n"
    "Check with your system's manufacturer for a BIOS update.\n",
    "Something went wrong with the kernel memory, and the pool allocator data has been "
    "corrupted.\n",
    "Either the kernel or a driver has tried freeing the same pool data twice.\n",
    "Either the kernel or a driver has tried freeing the same page twice.\n",
    "No memory left for an unpagable kernel allocation.\n",
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
    if (Message < KE_FATAL_ERROR || Message > KE_OUT_OF_MEMORY) {
        Message = KE_FATAL_ERROR;
    }

    VidSetColor(VID_COLOR_PANIC);
    VidPutString("CANNOT SAFELY RECOVER OPERATION\n");
    VidPutString(Messages[Message]);

    RtContext Context;
    RtSaveContext(&Context);
    VidPutString("\nSTACK TRACE:\n");

    do {
        KiDumpSymbol((void *)Context.Rip);
        uint64_t ImageBase = RtLookupImageBase(Context.Rip);
        RtVirtualUnwind(
            RT_UNW_FLAG_NHANDLER,
            ImageBase,
            Context.Rip,
            RtLookupFunctionEntry(ImageBase, Context.Rip),
            &Context,
            NULL,
            NULL);
    } while (Context.Rip >= MM_PAGE_SIZE);

    while (1)
        ;
}
