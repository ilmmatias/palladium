/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _PSP_H_
#define _PSP_H_

#include <evp.h>
#include <ps.h>

#define PSP_DEFAULT_TICKS ((10 * EV_MILLISECS) / EVP_TICK_PERIOD)

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

[[noreturn]] void PspInitializeScheduler(void);
void PspCreateIdleThread(void);
void PspCreateSystemThread(void);

void PspQueueThread(PsThread *Thread, bool EventQueue);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _PSP_H_ */
