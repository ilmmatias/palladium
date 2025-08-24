/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/kdp.h>
#include <kernel/ki.h>
#include <kernel/vidp.h>
#include <rt/except.h>

extern RtDList KiModuleListHead;

static const char *Messages[] = {
    "MANUALLY_INITIATED_CRASH",
    "IRQL_NOT_LESS_OR_EQUAL",
    "IRQL_NOT_GREATER_OR_EQUAL",
    "IRQL_NOT_EQUAL",
    "TRAP_NOT_HANDLED",
    "EXCEPTION_NOT_HANDLED",
    "PAGE_FAULT_NOT_HANDLED",
    "NMI_HARDWARE_FAILURE",
    "KERNEL_INITIALIZATION_FAILURE",
    "DRIVER_INITIALIZATION_FAILURE",
    "BAD_PFN_HEADER",
    "BAD_POOL_HEADER",
    "PROCESSOR_LIMIT_EXCEEDED",
    "BAD_THREAD_STATE",
    "MUTEX_NOT_OWNED",
};

static KeSpinLock Lock = {0};

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
    /* Let's prevent a possible exception/crash inside our crash handler. */
    if (Message >= KE_PANIC_COUNT) {
        Message = KE_PANIC_MANUALLY_INITIATED_CRASH;
    }

    /* Disable maskable interrupts, and raise the IRQL to the max (so we can be sure nothing
     * will interrupts us). */
    HalpEnterCriticalSection();
    KeSetIrql(KE_IRQL_MAX);

    /* Someone might have reached this handler before us (while we reached here before they sent
     * the panic event), hang ourselves if that't the case. */
    if (__atomic_fetch_or(&Lock, 0x01, __ATOMIC_ACQUIRE) & 0x01) {
        while (true) {
            StopProcessor();
        }
    }

    /* We're the first to get here, freeze everyone else before continuing. */
    HalpBroadcastFreeze();

    /* Acquire "ownership" of the display (disable the lock checks), setup the panic screen, and
     * show the basic message + error code. */
    VidpAcquireOwnership();
    VidSetColor(VID_COLOR_PANIC);
    VidResetDisplay();
    VidPrint("*** STOP: %s\n", Messages[Message]);
    KdpPrint(KDP_ANSI_FG_RED "*** STOP: %s" KDP_ANSI_RESET "\n", Messages[Message]);

    /* Dump all available parameters. */
    VidPrint(
        "*** PARAMETERS: %016llx, %016llx, %016llx, %016llx\n",
        Parameter1,
        Parameter2,
        Parameter3,
        Parameter4);
    KdpPrint(
        KDP_ANSI_FG_RED "*** PARAMETERS: %016llx, %016llx, %016llx, %016llx" KDP_ANSI_RESET "\n",
        Parameter1,
        Parameter2,
        Parameter3,
        Parameter4);

    /* We should be able to check for KiModuleListHead.Next's value to check if the boot module list
     * has already been saved (or if at least the kernel module itself has already been saved). */
    if (KiModuleListHead.Next != NULL && KiModuleListHead.Next != &KiModuleListHead) {
        /* And if so, we can print the backtrace of the first few frames from the stack (maybe move
         * the frame limit to a macro?) */
        void *Frames[32];
        int CapturedFrames = RtCaptureStackTrace(Frames, 32, 1);

        if (CapturedFrames) {
            VidPrint("*** STACK TRACE:\n");
            KdpPrint(KDP_ANSI_FG_RED "*** STACK TRACE:" KDP_ANSI_RESET "\n");

            for (int Frame = 0; Frame < CapturedFrames; Frame++) {
                KiDumpSymbol(Frames[Frame]);
            }
        } else {
            VidPutString("*** STACK TRACE NOT AVAILABLE\n");
            KdpPrint(KDP_ANSI_FG_RED "*** STACK TRACE NOT AVAILABLE" KDP_ANSI_RESET "\n");
        }

        /* Maybe we should add some output flag/signal to RtCaptureStackTrace so we know when
         * there is probably more frames avaliable? For now, this should be enough though. */
        if (CapturedFrames >= 32) {
            VidPrint("(more frames may follow...)\n");
            KdpPrint(KDP_ANSI_FG_RED "(more frames may follow...)" KDP_ANSI_RESET "\n");
        }
    } else {
        VidPutString("*** STACK TRACE NOT AVAILABLE\n");
        KdpPrint(KDP_ANSI_FG_RED "*** STACK TRACE NOT AVAILABLE" KDP_ANSI_RESET "\n");
    }

    while (true) {
        StopProcessor();
    }
}
