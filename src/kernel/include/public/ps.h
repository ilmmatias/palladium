/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _PS_H_
#define _PS_H_

#include <ev.h>
#include <generic/context.h>

typedef struct PsThread {
    RtDList ListHeader;
    uint64_t ExpirationReference;
    uint64_t ExpirationTicks;
    int Terminated;
    EvDpc TerminationDpc;
    HalContextFrame Context;
    char *Stack;
} PsThread;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

PsThread *PsCreateThread(void (*EntryPoint)(void *), void *Parameter);
void PsReadyThread(PsThread *Thread);
[[noreturn]] void PsTerminateThread(void);
void PsYieldExecution(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _PS_H_ */
