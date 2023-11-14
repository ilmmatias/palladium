/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _HAL_H_
#define _HAL_H_

#include <ke.h>
#include <ps.h>
#include <rt.h>

#define HAL_MICROSECS 1000ull
#define HAL_MILLISECS 1000000ull
#define HAL_SECS 1000000000ull

typedef struct {
    char Stack[0x4000];
    int Online;
    RtSList ListHeader;
    PsThread *InitialThread;
    PsThread *IdleThread;
    PsThread *CurrentThread;
    RtDList ThreadQueue;
    uint64_t ThreadQueueSize;
    KeSpinLock ThreadQueueLock;
    RtDList DpcQueue;
} HalProcessor;

HalProcessor *HalGetCurrentProcessor(void);

uint64_t HalGetTimerPeriod(void);
uint64_t HalGetTimerTicks(void);
void HalWaitTimer(uint64_t Time);

#endif /* _HAL_H_ */
