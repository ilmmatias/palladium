/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp.h>
#include <mm.h>
#include <psp.h>
#include <vid.h>

#include <cxx/lock.hxx>

extern "C" {
extern void KiContinueSystemStartup(void *);
extern void PspIdleThread(void *);

PsThread *PspSystemThread = NULL;
}

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
extern "C" PsThread *PsCreateThread(void (*EntryPoint)(void *), void *Parameter) {
    PsThread *Thread = (PsThread *)MmAllocatePool(sizeof(PsThread), "Ps  ");
    if (!Thread) {
        return NULL;
    }

    Thread->Stack = (char *)MmAllocatePool(KE_STACK_SIZE, "Ps  ");
    if (!Thread->Stack) {
        MmFreePool(Thread, "Ps  ");
        return NULL;
    }

    HalpInitializeThreadContext(
        &Thread->Context, Thread->Stack, KE_STACK_SIZE, EntryPoint, Parameter);

    return Thread;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function marks the current thread for deletion, and yields out into the next thread.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
extern "C" [[noreturn]] void PsTerminateThread(void) {
    KeProcessor *Processor = HalGetCurrentProcessor();
    PsThread *Thread = Processor->CurrentThread;
    Thread->Terminated = 1;
    HalpNotifyProcessor(Processor, 1);
    while (1) {
        HalpStopProcessor();
    }
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
extern "C" void PspCreateSystemThread(void) {
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
extern "C" void PspCreateIdleThread(void) {
    HalGetCurrentProcessor()->IdleThread = PsCreateThread(PspIdleThread, NULL);
    if (!HalGetCurrentProcessor()->IdleThread) {
        VidPrint(VID_MESSAGE_ERROR, "Kernel", "failed to create the idle thread\n");
        KeFatalError(KE_OUT_OF_MEMORY);
    }
}
