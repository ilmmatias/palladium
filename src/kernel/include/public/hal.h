/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _HAL_H_
#define _HAL_H_

#include <ev.h>
#include <ke.h>
#include <ps.h>

typedef struct {
    char Stack[0x4000];

    int Online;
    RtSList ListHeader;

    PsThread *InitialThread;
    PsThread *IdleThread;
    PsThread *CurrentThread;
    RtDList ThreadQueue;
    size_t ThreadQueueSize;
    KeSpinLock ThreadQueueLock;

    int ForceYield;
    uint64_t ClosestEvent;
    RtDList DpcQueue;
    RtDList EventQueue;
} HalProcessor;

HalProcessor *HalGetCurrentProcessor(void);

void HalSaveContext(HalRegisterState *Context);
void HalRestoreContext(HalRegisterState *Context);

uint64_t HalGetTimerPeriod(void);
uint64_t HalGetTimerTicks(void);
void HalWaitTimer(uint64_t Time);

#endif /* _HAL_H_ */
