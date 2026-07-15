/* SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/ke.h>
#include <kernel/mm.h>
#include <kernel/ob.h>
#include <kernel/obp.h>
#include <stddef.h>
#include <stdint.h>
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
    uint32_t References = __atomic_load_n(&Object->References, __ATOMIC_ACQUIRE);
    while (true) {
        if (!References || References == UINT32_MAX) {
            KeFatalError(KE_PANIC_BAD_OBJECT_REFERENCE_COUNT, (uint64_t)Body, References, 1, 0);
        }

        if (__atomic_compare_exchange_n(
                &Object->References,
                &References,
                References + 1,
                true,
                __ATOMIC_ACQ_REL,
                __ATOMIC_ACQUIRE)) {
            return;
        }
    }
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
    uint32_t References = __atomic_load_n(&Object->References, __ATOMIC_ACQUIRE);
    while (true) {
        if (!References) {
            KeFatalError(KE_PANIC_BAD_OBJECT_REFERENCE_COUNT, (uint64_t)Body, References, 0, 0);
        }

        uint32_t NewReferences = References - 1;
        if (!__atomic_compare_exchange_n(
                &Object->References,
                &References,
                NewReferences,
                true,
                __ATOMIC_ACQ_REL,
                __ATOMIC_ACQUIRE)) {
            continue;
        }

        if (!NewReferences) {
            if (Object->Type->Delete) {
                Object->Type->Delete(Body);
            }

            MmFreePool(Object, Object->Tag);
        }
        return;
    }
}
