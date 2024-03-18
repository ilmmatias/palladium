/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _PS_H_
#define _PS_H_

#if defined(ARCH_amd64)
#include <amd64/regs.h>
#else
#error "Undefined ARCH for the kernel module!"
#endif /* ARCH */

#include <ev.h>

typedef struct PsThread {
    RtDList ListHeader;
    uint64_t Expiration;
    int Terminated;
    EvDpc TerminationDpc;
    HalRegisterState Context;
    char *Stack;
} PsThread;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

PsThread *PsCreateThread(void (*EntryPoint)(void *), void *Parameter);
void PsReadyThread(PsThread *Thread);
[[noreturn]] void PsTerminateThread(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _PS_H_ */
