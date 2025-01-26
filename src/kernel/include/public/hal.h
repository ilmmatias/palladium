/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _HAL_H_
#define _HAL_H_

#include <ke.h>
#include <ps.h>

#define HAL_NO_EVENT 0
#define HAL_PANIC_EVENT 1

#define HAL_TIMER_WIDTH_32B 32
#define HAL_TIMER_WIDTH_64B 64

#define HAL_INT_TYPE_EDGE 0
#define HAL_INT_TYPE_LEVEL 1

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct {
    RtDList ListHeader;
    int Enabled;
    KeSpinLock Lock;
    KeIrql Irql;
    uint32_t Vector;
    uint8_t Type;
    void (*Handler)(HalInterruptFrame *, void *);
    void *HandlerContext;
} HalInterrupt;

KeProcessor *HalGetCurrentProcessor(void);

int HalGetTimerWidth(void);
uint64_t HalGetTimerPeriod(void);
uint64_t HalGetTimerTicks(void);
int HalCheckTimerExpiration(uint64_t Current, uint64_t Reference, uint64_t Ticks);
void HalWaitTimer(uint64_t Time);

HalInterrupt *HalCreateInterrupt(
    KeIrql Irql,
    uint32_t Vector,
    uint8_t Type,
    void (*Handler)(HalInterruptFrame *, void *),
    void *HandlerContext);
int HalEnableInterrupt(HalInterrupt *Interrupt);
void HalDisableInterrupt(HalInterrupt *Interrupt);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _HAL_H_ */
