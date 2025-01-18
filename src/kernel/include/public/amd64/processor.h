/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _AMD64_PROCESSOR_H_
#define _AMD64_PROCESSOR_H_

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
    struct __attribute__((packed)) {
        uint16_t BaseLow;
        uint16_t Cs;
        uint8_t Ist;
        uint8_t Attributes;
        uint16_t BaseMid;
        uint32_t BaseHigh;
        uint32_t Reserved;
    } IdtEntries[256];
    struct __attribute__((packed)) __attribute__((aligned(4))) {
        uint16_t Limit;
        uint64_t Base;
    } IdtDescriptor;
    struct {
        RtSList ListHead;
        uint32_t Usage;
    } IdtSlots[224];
    uint64_t IdtIrqlSlots[256];
} KeProcessor;

#endif /* _AMD64_PROCESSOR_H_ */
