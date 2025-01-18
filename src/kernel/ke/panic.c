/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp.h>
#include <ki.h>
#include <mm.h>
#include <rt/except.h>
#include <vidp.h>

static const char *Messages[] = {
    "MANUALLY_INITIATED_CRASH",
    "IRQL_NOT_LESS_OR_EQUAL",
    "IRQL_NOT_GREATER_OR_EQUAL",
    "IRQL_NOT_DISPATCH",
    "SPIN_LOCK_ALREADY_OWNED",
    "SPIN_LOCK_NOT_OWNED",
    "EXCEPTION_NOT_HANDLED",
    "TRAP_NOT_HANDLED",
    "PAGE_FAULT_NOT_HANDLED",
    "SYSTEM_SERVICE_NOT_HANDLED",
    "NMI_HARDWARE_FAILURE",
    "INSTALL_MORE_MEMORY",
    "BAD_PFN_HEADER",
    "BAD_POOL_HEADER",
    "BAD_SYSTEM_TABLE",
};

static uint64_t Lock = 0;

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
[[noreturn]] void KeFatalError(uint32_t Message) {
    KeProcessor *Processor = (KeProcessor *)HalGetCurrentProcessor();

    /* Let's prevent a possible exception/crash inside our crash handler. */
    if (Message >= KE_PANIC_COUNT) {
        Message = KE_PANIC_MANUALLY_INITIATED_CRASH;
    }

    /* Disable maskable interrupts, and raise the IRQL to the max (so we can be sure nothing
     * will interrupts us). */
    HalpEnterCriticalSection();
    HalpSetIrql(KE_IRQL_DISPATCH);

    /* Someone might have reached this handler before us (while we reached here before they sent
     * the panic event), hang ourselves if that't the case. */
    if (__atomic_fetch_add(&Lock, 1, __ATOMIC_SEQ_CST)) {
        while (1) {
            HalpStopProcessor();
        }
    }

    /* We're the first to get here, freeze everyone else before continuing. */
    for (uint32_t i = 0; i < HalpProcessorCount; i++) {
        if (HalpProcessorList[i] != Processor) {
            HalpFreezeProcessor(HalpProcessorList[i]);
        }
    }

    /* Setup the panic screen, and show the basic message + error code. */
    VidSetColor(VID_COLOR_PANIC);
    VidResetDisplay();
    VidPutString("*** STOP: ");
    VidPutString(Messages[Message]);
    VidPutString("\n");

    /* And a backtrace of all frames we can obtain from the stack. */
    VidPutString("*** STACK TRACE:\n");
    RtContext Context;
    RtSaveContext(&Context);
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
