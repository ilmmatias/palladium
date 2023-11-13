/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _TIMER_H_
#define _TIMER_H_

#include <stdint.h>

void BmSetupTimer(void);
uint64_t BmGetElapsedTime(void);

#endif /* _TIMER_H_ */
