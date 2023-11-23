/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _PS_H_
#define _PS_H_

#if defined(ARCH_x86) || defined(ARCH_amd64)
#include <amd64/regs.h>
#else
#error "Undefined ARCH for the kernel module!"
#endif /* ARCH */

#include <rt/list.h>

typedef struct {
    RtDList ListHeader;
    char *Stack;
    uint64_t Expiration;
    HalRegisterState Context;
} PsThread;

PsThread *PsCreateThread(void (*EntryPoint)(void *), void *Parameter);
void PsReadyThread(PsThread *Thread);

#endif /* _PS_H_ */
