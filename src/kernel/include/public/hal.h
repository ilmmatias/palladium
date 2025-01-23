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

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

KeProcessor *HalGetCurrentProcessor(void);

int HalGetTimerWidth(void);
uint64_t HalGetTimerPeriod(void);
uint64_t HalGetTimerTicks(void);
int HalCheckTimerExpiration(uint64_t Current, uint64_t Reference, uint64_t Ticks);
void HalWaitTimer(uint64_t Time);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _HAL_H_ */
