/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _EV_H_
#define _EV_H_

#include <ps.h>

#define EV_TYPE_TIMER 0

#define EV_MICROSECS 1000ull
#define EV_MILLISECS 1000000ull
#define EV_SECS 1000000000ull

/* Deferred Procedure Call; We're the only event that doesn't have an event header ourselves. */
typedef struct {
    RtDList ListHeader;
    void (*Routine)(void *Context);
    void *Context;
} EvDpc;

typedef struct {
    RtDList ListHeader;
    int Type;
    int Dispatched;
    int Finished;
    PsThread *Source;
    EvDpc *Dpc;
    uint64_t Deadline;
} EvHeader, EvTimer;

void EvInitializeDpc(EvDpc *Dpc, void (*Routine)(void *Context), void *Context);
void EvDispatchDpc(EvDpc *Dpc);

void EvInitializeTimer(EvTimer *Timer, uint64_t Timeout, EvDpc *Dpc);

void EvWaitObject(void *Object, uint64_t Timeout);

#endif /* _EV_H_ */
