/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _PSP_H_
#define _PSP_H_

#include <ps.h>

#define PSP_THREAD_QUANTUM (10 * HAL_MILLISECS)

void PspCreateSystemThread(void);
void PspCreateIdleThread(void);
void PspInitializeScheduler(int IsBsp);

#endif /* _PSP_H_ */
