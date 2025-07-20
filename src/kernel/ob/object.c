/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/ke.h>
#include <kernel/mm.h>
#include <kernel/obp.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function allocates and sets up a new managed object pointer.
 *
 * PARAMETERS:
 *     Type - Pointer to the type of the object.
 *     Tag - Name/identifier to be attached to the block.
 *
 * RETURN VALUE:
 *     Either the start of the usable object data/body, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
void *ObCreateObject(ObType *Type, char Tag[4]) {
    ObpObject *Object = MmAllocatePool(sizeof(ObpObject) + Type->Size, Tag);
    if (!Object) {
        return NULL;
    }

    /* Initialization of the object data should be done by the caller, not us, so this is all we
     * need to do. */
    Object->Type = Type;
    Object->References = 1;
    memcpy(Object->Tag, Tag, 4);
    return Object + 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function adds a reference to the given object.
 *
 * PARAMETERS:
 *     Body - The pointer to the body of the object we should add the reference to.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void ObReferenceObject(void *Body) {
    ObpObject *Object = (ObpObject *)Body - 1;
    __atomic_add_fetch(&Object->References, 1, __ATOMIC_RELEASE);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function removes a reference from the given object, calling the destructor of the
 *     object if required.
 *
 * PARAMETERS:
 *     Body - The pointer to the body of the object we should remove the reference from.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void ObDereferenceObject(void *Body) {
    ObpObject *Object = (ObpObject *)Body - 1;
    if (!__atomic_sub_fetch(&Object->References, 1, __ATOMIC_ACQUIRE)) {
        /* Should we allow the delete pointer to be NULL? */
        if (Object->Type->Delete) {
            Object->Type->Delete(Body);
        }

        MmFreePool(Object, Object->Tag);
    }
}
