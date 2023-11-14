/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _EV_H_
#define _EV_H_

#include <rt.h>

typedef struct {
    RtDList ListHeader;
    void (*Routine)(void *Context);
    void *Context;
} EvDpc;

void EvInitializeDpc(EvDpc *Dpc, void (*Routine)(void *Context), void *Context);
void EvDispatchDpc(EvDpc *Dpc);

#endif /* _EV_H_ */
