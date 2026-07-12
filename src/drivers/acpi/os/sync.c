/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <acpip.h>
#include <kernel/ev.h>
#include <stdint.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates a kernel mutex object for use as an ACPI Mutex.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Pointer to the kernel mutex object, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
void *AcpipCreateMutex(void) {
    return EvCreateMutex();
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function acquires an ACPI Mutex with an optional timeout.
 *
 * PARAMETERS:
 *     Mutex - Pointer to the kernel mutex object.
 *     Timeout - Timeout in milliseconds; 0xFFFF means wait indefinitely.
 *
 * RETURN VALUE:
 *     0 on success (mutex acquired), non-zero on timeout.
 *-----------------------------------------------------------------------------------------------*/
bool AcpipAcquireMutex(void *Mutex, uint64_t Timeout) {
    return EvAcquireMutex(Mutex, Timeout == 0xFFFF ? EV_TIMEOUT_UNLIMITED : Timeout * EV_MILLISECS);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function releases an ACPI Mutex.
 *
 * PARAMETERS:
 *     Mutex - Pointer to the kernel mutex object.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpipReleaseMutex(void *Mutex) {
    EvReleaseMutex(Mutex);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates a kernel signal object for use as an ACPI Event.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Pointer to the kernel signal object, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
void *AcpipCreateEvent(void) {
    return EvCreateSignal();
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function waits for an ACPI Event to be signaled, with an optional timeout.
 *
 * PARAMETERS:
 *     Event - Pointer to the kernel signal object.
 *     Timeout - Timeout in milliseconds; 0xFFFF means wait indefinitely.
 *
 * RETURN VALUE:
 *     0 on success (event signaled), non-zero on timeout.
 *-----------------------------------------------------------------------------------------------*/
bool AcpipWaitEvent(void *Event, uint64_t Timeout) {
    return EvWaitForObject(
        Event, Timeout == 0xFFFF ? EV_TIMEOUT_UNLIMITED : Timeout * EV_MILLISECS);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function signals an ACPI Event, waking the next waiting thread.
 *
 * PARAMETERS:
 *     Event - Pointer to the kernel signal object.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpipSignalEvent(void *Event) {
    EvSetSignal(Event);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function resets an ACPI Event, clearing the signaled state.
 *
 * PARAMETERS:
 *     Event - Pointer to the kernel signal object.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpipResetEvent(void *Event) {
    EvClearSignal(Event);
}
