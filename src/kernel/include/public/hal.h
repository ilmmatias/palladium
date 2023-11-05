/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _HAL_H_
#define _HAL_H_

#include <stdint.h>

#define HAL_MICROSECS 1000ull
#define HAL_MILLISECS 1000000ull
#define HAL_SECS 1000000000ull

void *HalFindAcpiTable(const char Signature[4], int Index);

uint64_t HalGetTimerPeriod(void);
uint64_t HalGetTimerTicks(void);
void HalWaitTimer(uint64_t Time);

#endif /* _HAL_H_ */
