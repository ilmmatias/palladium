/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _PSP_H_
#define _PSP_H_

#include <hal.h>
#include <ps.h>

#define PSP_THREAD_QUANTUM (10 * HAL_MILLISECS)
#define PSP_THREAD_MIN_QUANTUM (750 * HAL_MICROSECS)

void PspCreateSystemThread(void);
void PspCreateIdleThread(void);
void PspInitializeScheduler(int IsBsp);
void PspScheduleNext(HalRegisterState *Context);

#endif /* _PSP_H_ */
