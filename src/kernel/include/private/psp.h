/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _PSP_H_
#define _PSP_H_

#include <hal.h>
#include <ps.h>

#define PSP_THREAD_QUANTUM (10 * EV_MILLISECS)
#define PSP_THREAD_MIN_QUANTUM (1 * EV_MILLISECS)

#define PSP_YIELD_NONE 0
#define PSP_YIELD_REQUEST 1
#define PSP_YIELD_EVENT 2

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void PspCreateSystemThread(void);
void PspCreateIdleThread(void);
void PspInitializeScheduler(int IsBsp);
void PspScheduleNext(HalRegisterState *Context);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _PSP_H_ */
