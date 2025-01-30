/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _AMD64_PROCESSOR_H_
#define _AMD64_PROCESSOR_H_

#include <amd64/descr.h>
#include <amd64/msr.h>

typedef struct {
    KeSpinLock Lock;
    uint32_t ApicId;
    RtDList WaitQueue;
    RtDList ThreadQueue;
    RtDList TerminationQueue;
    PsThread *CurrentThread;
    PsThread *IdleThread;
    int EventType;
    char *StackBase;
    char *StackLimit;
    char SystemStack[8192] __attribute__((aligned(4096)));
    char NmiStack[8192] __attribute__((aligned(4096)));
    char DoubleFaultStack[8192] __attribute__((aligned(4096)));
    char MachineCheckStack[8192] __attribute__((aligned(4096)));
    char GdtEntries[56];
    HalpTssEntry TssEntry;
    HalpIdtEntry IdtEntries[256];
    RtDList InterruptList[256];
} KeProcessor;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function gets a pointer to the processor-specific structure of the current processor.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Pointer to the processor struct.
 *-----------------------------------------------------------------------------------------------*/
static inline KeProcessor *KeGetCurrentProcessor(void) {
    return (KeProcessor *)ReadMsr(0xC0000102);
}

#endif /* _AMD64_PROCESSOR_H_ */
