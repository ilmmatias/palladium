/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _PSP_H_
#define _PSP_H_

#include <hal.h>
#include <ps.h>

#define PSP_THREAD_QUANTUM (10 * EV_MILLISECS)
#define PSP_THREAD_MIN_QUANTUM (1 * EV_MILLISECS)

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void PspCreateSystemThread(void);
void PspCreateIdleThread(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _PSP_H_ */
