/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _AMD64_PROCESSOR_H_
#define _AMD64_PROCESSOR_H_

#include <amd64/descr.h>
#include <ps.h>
#include <rt/list.h>

typedef struct {
    uint32_t ApicId;
    uint64_t ThreadQueueLock;
    RtDList ThreadQueue;
    uint32_t ThreadQueueSize;
    PsThread *InitialThread;
    PsThread *CurrentThread;
    PsThread *IdleThread;
    int EventStatus;
    RtDList DpcQueue;
    RtDList EventQueue;
    char SystemStack[8192] __attribute__((aligned(4096)));
    char GdtEntries[56];
    HalpTssEntry TssEntry;
    HalpIdtEntry IdtEntries[256];
    uint64_t InterruptListLock;
    RtDList InterruptList[256];
} KeProcessor;

#endif /* _AMD64_PROCESSOR_H_ */
