/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _EV_H_
#define _EV_H_

#include <rt/list.h>

#define EV_TYPE_TIMER 0

#define EV_MICROSECS 1000ull
#define EV_MILLISECS 1000000ull
#define EV_SECS 1000000000ull

struct PsThread;

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
    struct PsThread *Source;
    EvDpc *Dpc;
    uint64_t Deadline;
} EvHeader, EvTimer;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void EvInitializeDpc(EvDpc *Dpc, void (*Routine)(void *Context), void *Context);
void EvDispatchDpc(EvDpc *Dpc);

void EvInitializeTimer(EvTimer *Timer, uint64_t Timeout, EvDpc *Dpc);

void EvWaitObject(void *Object, uint64_t Timeout);
void EvCancelObject(void *Object);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _EV_H_ */
