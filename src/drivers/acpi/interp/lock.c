/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <acpip.h>
#include <stddef.h>
#include <stdint.h>

static SdtHeader *FacsTable = NULL;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function grabs up the FACS table (which contains the _GL data).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpipInitializeGlobalLock(void) {
    FacsTable = AcpipFindTable("FACS", 0);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries acquiring the _GL from the firmware.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     true/false depending on success.
 *-----------------------------------------------------------------------------------------------*/
bool AcpipAcquireGlobalLock(void) {
    if (!FacsTable) {
        return true;
    }

    /* TODO: Probably add a FACS struct to sdt.h? */
    volatile uint32_t *Lock = (volatile uint32_t *)((uintptr_t)FacsTable + 16);
    uint32_t OldValue, NewValue;

    /* And this stuff (the magic constants) probably deserve macros or struct/unions... */
    do {
        OldValue = *Lock;
        NewValue = (OldValue & ~0x03) | 0x02;
        if (OldValue & 0x02) {
            NewValue |= 0x01;
        }
    } while (!__atomic_compare_exchange_n(
        Lock, &OldValue, NewValue, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE));

    return !((OldValue & 0x02) >> 1);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function releases the _GL back to the firmware.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     true if the firmware wants notification (pending was set), false otherwise.
 *-----------------------------------------------------------------------------------------------*/
bool AcpipReleaseGlobalLock(void) {
    if (!FacsTable) {
        return false;
    }

    volatile uint32_t *Lock = (volatile uint32_t *)((uintptr_t)FacsTable + 16);
    uint32_t OldValue, NewValue;

    do {
        OldValue = *Lock;
        NewValue = OldValue & ~0x03;
    } while (!__atomic_compare_exchange_n(
        Lock, &OldValue, NewValue, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE));

    return OldValue & 0x01;
}
