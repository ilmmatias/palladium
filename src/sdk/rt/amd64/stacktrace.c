/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/ke.h>
#include <rt/except.h>

//
#include <kernel/vid.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function unwinds the caller's stack, capturing all frames along the way.
 *
 * PARAMETERS:
 *     Frames - Where to store the frames.
 *     FramesToCapture - How many addresses we should store in the Frames array.
 *     FramesToSkip - How many addresses we should skip before starting to fill the Frames array.
 *
 * RETURN VALUE:
 *     How many frames we actually captured.
 *-----------------------------------------------------------------------------------------------*/
int RtCaptureStackTrace(void **Frames, int FramesToCapture, int FramesToSkip) {
    RtContext Context;
    RtSaveContext(&Context);

    /* We need this to validate the stack frames. */
    KeProcessor *Processor = KeGetCurrentProcessor();
    uint64_t StackBase = (uint64_t)Processor->StackBase;
    uint64_t StackLimit = (uint64_t)Processor->StackLimit;
    int CapturedFrames = 0;

    while (Context.Rsp >= StackBase && Context.Rsp < StackLimit) {
        uint64_t ImageBase = RtLookupImageBase(Context.Rip);
        if (!ImageBase) {
            /* We probably unwound past the actual stack limits (or something else got corrupted)?
             * Well, just return early here. */
            break;
        }

        RtRuntimeFunction *FunctionEntry = RtLookupFunctionEntry(ImageBase, Context.Rip);
        if (FunctionEntry) {
            RtVirtualUnwind(
                RT_UNW_FLAG_NHANDLER, ImageBase, Context.Rip, FunctionEntry, &Context, NULL, NULL);
        } else {
            /* Leaf function, manually update the Rip and Rsp values. */
            Context.Rip = *(uint64_t *)Context.Rsp;
            Context.Rsp += sizeof(uint64_t);
        }

        /* We can stop if we left kernel mode (we should add support for user mode addresses later
         * though!) */
        if (Context.Rip < 0xFFFF800000000000) {
            break;
        }

        /* Don't do anything if we're still skipping some frames. */
        if (FramesToSkip) {
            FramesToSkip--;
        } else {
            Frames[CapturedFrames++] = (void *)Context.Rip;
            if (CapturedFrames >= FramesToCapture) {
                break;
            }
        }
    }

    return CapturedFrames;
}
