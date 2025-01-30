/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _PS_H_
#define _PS_H_

#include <generic/context.h>
#include <rt/list.h>

#define PS_YIELD_TYPE_QUEUE 0x00
#define PS_YIELD_TYPE_UNQUEUE 0x01

typedef struct {
    RtDList ListHeader;
    uint64_t ExpirationTicks;
    uint64_t WaitTicks;
    char *Stack;
    char *StackLimit;
    HalContextFrame ContextFrame;
} PsThread;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

PsThread *PsCreateThread(void (*EntryPoint)(void *), void *Parameter);
void PsQueueThread(PsThread *Thread);
[[noreturn]] void PsTerminateThread(void);
void PsYieldThread(int Type);
void PsDelayThread(uint64_t Time);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _PS_H_ */
