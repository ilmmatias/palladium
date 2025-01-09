/* SPDX-FileCopyrightText: (C) 2024-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _AMD64_PROCESSOR_H_
#define _AMD64_PROCESSOR_H_

#include <rt/list.h>

/* Copy of the KeProcessor struct from src/kernel/include/public/amd64/processor.h. */
typedef struct {
    uint32_t ApicId;
    int ThreadQueueLock;
    RtDList ThreadQueue;
    uint32_t ThreadQueueSize;
    void *InitialThread;
    void *CurrentThread;
    void *IdleThread;
    int ForceYield;
    int EventStatus;
    RtDList DpcQueue;
    RtDList EventQueue;
    char SystemStack[8192] __attribute__((aligned(4096)));
    uint64_t GdtEntries[5];
    struct __attribute__((packed)) __attribute__((aligned(4))) {
        uint16_t Limit;
        uint64_t Base;
    } GdtDescriptor;
    char IdtEntries[4096];
    struct __attribute__((packed)) __attribute__((aligned(4))) {
        uint16_t Limit;
        uint64_t Base;
    } IdtDescriptor;
    struct {
        RtSList ListHead;
        uint32_t Usage;
    } IdtSlots[224];
    uintptr_t IdtIrqlSlots[256];
} KeProcessor;

#endif /* _AMD64_PROCESSOR_H_ */
