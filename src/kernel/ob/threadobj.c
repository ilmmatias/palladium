/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/mm.h>
#include <kernel/ob.h>
#include <kernel/ps.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function cleans up a thread object after all references to it have been removed.
 *
 * PARAMETERS:
 *     Object - Which object to clean up.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void DeleteRoutine(void *Object) {
    PsThread *Thread = Object;
    if (Thread->AllocatedStack) {
        MmFreePool(Thread->AllocatedStack, MM_POOL_TAG_KERNEL_STACK);
    }
}

ObType ObpThreadType = {
    .Name = "Thread",
    .Size = sizeof(PsThread),
    .Delete = DeleteRoutine,
};
