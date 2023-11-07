/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <halp.h>
#include <ke.h>
#include <mm.h>
#include <ps.h>
#include <vid.h>

extern void KiContinueSystemStartup(void *);
extern void PspIdleThread(void *);

PsThread *PspSystemThread = NULL;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates and initializes a new thread.
 *     This won't add it to the current processor's ready list, use PsReadyThread() for that.
 *
 * PARAMETERS:
 *     EntryPoint - Where the thread should jump on first execution.
 *     Parameter - What to pass into the thread entry point.
 *
 * RETURN VALUE:
 *     Pointer to the thread structure, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
PsThread *PsCreateThread(void (*EntryPoint)(void *), void *Parameter) {
    PsThread *Thread = MmAllocatePool(sizeof(PsThread), "Ps  ");
    if (!Thread) {
        return NULL;
    }

    Thread->Stack = MmAllocatePool(KE_STACK_SIZE, "Ps  ");
    if (!Thread->Stack) {
        MmFreePool(Thread, "Ps  ");
        return NULL;
    }

    HalpInitializeContext(&Thread->Context, Thread->Stack, KE_STACK_SIZE, EntryPoint, Parameter);
    return Thread;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates the system thread. We should only be called by the BSP.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void PspCreateSystemThread(void) {
    PspSystemThread = PsCreateThread(KiContinueSystemStartup, NULL);
    if (!PspSystemThread) {
        VidPrint(VID_MESSAGE_ERROR, "Kernel", "failed to create the system thread\n");
        KeFatalError(KE_OUT_OF_MEMORY);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates the idle thread for this processor.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void PspCreateIdleThread(void) {
    HalGetCurrentProcessor()->IdleThread = PsCreateThread(PspIdleThread, NULL);
    if (!HalGetCurrentProcessor()->IdleThread) {
        VidPrint(VID_MESSAGE_ERROR, "Kernel", "failed to create the idle thread\n");
        KeFatalError(KE_OUT_OF_MEMORY);
    }
}
