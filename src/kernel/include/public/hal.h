/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _HAL_H_
#define _HAL_H_

#include <ke.h>
#include <ps.h>

#define HAL_NO_EVENT 0
#define HAL_PANIC_EVENT 1

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

KeProcessor *HalGetCurrentProcessor(void);

uint64_t HalGetTimerPeriod(void);
uint64_t HalGetTimerTicks(void);
void HalWaitTimer(uint64_t Time);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _HAL_H_ */
