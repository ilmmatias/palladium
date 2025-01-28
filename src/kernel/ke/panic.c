/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp.h>
#include <ki.h>
#include <mm.h>
#include <rt/except.h>
#include <stdio.h>
#include <vidp.h>

static const char *Messages[] = {
    "MANUALLY_INITIATED_CRASH",
    "IRQL_NOT_LESS_OR_EQUAL",
    "IRQL_NOT_GREATER_OR_EQUAL",
    "IRQL_NOT_DISPATCH",
    "SPIN_LOCK_ALREADY_OWNED",
    "SPIN_LOCK_NOT_OWNED",
    "TRAP_NOT_HANDLED",
    "EXCEPTION_NOT_HANDLED",
    "PAGE_FAULT_NOT_HANDLED",
    "NMI_HARDWARE_FAILURE",
    "KERNEL_INITIALIZATION_FAILURE",
    "DRIVER_INITIALIZATION_FAILURE",
    "BAD_PFN_HEADER",
    "BAD_POOL_HEADER",
};

static uint64_t Lock = 0;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function takes over the display, and writes a fatal error message (panic), followed by
 *     halting the system.
 *
 * PARAMETERS:
 *     Message - Number of the error code/message to be shown.
 *     Parameter1 - Parameter to help with debugging the panic reason.
 *     Parameter2 - Parameter to help with debugging the panic reason.
 *     Parameter3 - Parameter to help with debugging the panic reason.
 *     Parameter4 - Parameter to help with debugging the panic reason.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void KeFatalError(
    uint32_t Message,
    uint64_t Parameter1,
    uint64_t Parameter2,
    uint64_t Parameter3,
    uint64_t Parameter4) {
    KeProcessor *Processor = HalGetCurrentProcessor();

    /* Let's prevent a possible exception/crash inside our crash handler. */
    if (Message >= KE_PANIC_COUNT) {
        Message = KE_PANIC_MANUALLY_INITIATED_CRASH;
    }

    /* Disable maskable interrupts, and raise the IRQL to the max (so we can be sure nothing
     * will interrupts us). */
    HalpEnterCriticalSection();
    HalpSetIrql(KE_IRQL_MAX);

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

    /* Acquire "ownership" of the display (disable the lock checks), setup the panic screen, and
     * show the basic message + error code. */
    VidpAcquireOwnership();
    VidSetColor(VID_COLOR_PANIC);
    VidResetDisplay();
    VidPutString("*** STOP: ");
    VidPutString(Messages[Message]);
    VidPutChar('\n');

    /* Dump all available parameters. */
    char String[128];
    snprintf(
        String,
        sizeof(String),
        "*** PARAMETERS: 0x%016llx, 0x%016llx, 0x%016llx, 0x%016llx\n",
        Parameter1,
        Parameter2,
        Parameter3,
        Parameter4);
    VidPutString(String);

    /* And a backtrace of all frames we can obtain from the stack.
     * TODO: We should move all of this to a RtSaveStackTrace function, as it uses arch-specific
     * knowledge (Context.Rip and Context.Rsp). */
    VidPutString("*** STACK TRACE:\n");

    /* Capture the current context (RtVirtualUnwind uses it). */
    RtContext Context;
    RtSaveContext(&Context);

    /* Get the stack limits (without them we'll probably crash). */
    uint64_t StackBase = 0;
    uint64_t StackLimit = 0;
    if (Processor->CurrentThread) {
        /* TODO: We should probably handle the DPC stack and any other kernel stack). */
        StackBase = (uint64_t)Processor->CurrentThread->Stack;
        StackLimit = StackBase + KE_STACK_SIZE;
    } else if (Processor) {
        StackBase = (uint64_t)Processor->SystemStack;
        StackLimit = StackBase + KE_STACK_SIZE;
    } else {
        StackBase = 0;
        StackLimit = UINT64_MAX;
    }

    if (Context.Rsp < StackBase || Context.Rsp >= StackLimit) {
        StackBase = 0;
        StackLimit = UINT64_MAX;
    }

    /* And start walking the unwind data (until we either leave the kernel space, or leave the
     * kernel stack). */
    while (1) {
        KiDumpSymbol((void *)Context.Rip);

        uint64_t ImageBase = RtLookupImageBase(Context.Rip);
        if (!ImageBase) {
            break;
        }

        RtRuntimeFunction *FunctionEntry = RtLookupFunctionEntry(ImageBase, Context.Rip);
        if (FunctionEntry) {
            RtVirtualUnwind(
                RT_UNW_FLAG_NHANDLER, ImageBase, Context.Rip, FunctionEntry, &Context, NULL, NULL);
        } else {
            Context.Rip = *(uint64_t *)Context.Rsp;
            Context.Rsp += sizeof(uint64_t);
        }

        if (Context.Rip < 0xFFFF800000000000 || Context.Rsp < StackBase ||
            Context.Rsp >= StackLimit) {
            break;
        }
    }

    while (1) {
        HalpStopProcessor();
    }
}
