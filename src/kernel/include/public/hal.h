/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _HAL_H_
#define _HAL_H_

#include <ke.h>
#include <ps.h>

#define HAL_NO_EVENT 0
#define HAL_PANIC_EVENT 1

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

    int EventStatus;
    int ForceYield;
    uint64_t ClosestEvent;
    RtDList DpcQueue;
    RtDList EventQueue;
} HalProcessor;

HalProcessor *HalGetCurrentProcessor(void);

uint64_t HalGetTimerPeriod(void);
uint64_t HalGetTimerTicks(void);
void HalWaitTimer(uint64_t Time);

#endif /* _HAL_H_ */
