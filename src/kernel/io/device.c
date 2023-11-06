/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <io.h>
#include <ke.h>
#include <mm.h>
#include <rt.h>
#include <string.h>

static RtSList DeviceListHead = {.Next = NULL};
static KeSpinLock Lock = {0};

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function registers a new device, using the specified unique name.
 *
 * PARAMETERS:
 *     Name - Unique name for the device.
 *     Read - Handler for read operations on the device; Cannot be NULL.
 *     Write - Handler for write operations on the device; Cannot be NULL.
 *
 * RETURN VALUE:
 *     1 on success, 0 on failure (either out of memory, or the name already exists).
 *-----------------------------------------------------------------------------------------------*/
int IoCreateDevice(const char *Name, IoReadFn Read, IoWriteFn Write) {
    if (IoOpenDevice(Name)) {
        return 0;
    }

    IoDevice *Entry = MmAllocatePool(sizeof(IoDevice), "Io  ");
    if (!Entry) {
        return 0;
    }

    Entry->Name = MmAllocatePool(strlen(Name) + 1, "Io  ");
    if (!Entry->Name) {
        MmFreePool(Entry, "Io  ");
        return 0;
    }

    strcpy((char *)Entry->Name, Name);
    Entry->Read = Read;
    Entry->Write = Write;

    KeAcquireSpinLock(&Lock);
    RtPushSList(&DeviceListHead, &Entry->ListHeader);
    KeReleaseSpinLock(&Lock);

    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries opening a previously registered device.
 *
 * PARAMETERS:
 *     Name - Device name.
 *
 * RETURN VALUE:
 *     Pointer to the device on success, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
IoDevice *IoOpenDevice(const char *Name) {
    KeAcquireSpinLock(&Lock);

    RtSList *ListHeader = DeviceListHead.Next;
    while (ListHeader) {
        IoDevice *Entry = CONTAINING_RECORD(ListHeader, IoDevice, ListHeader);
        if (!strcmp(Entry->Name, Name)) {
            KeReleaseSpinLock(&Lock);
            return Entry;
        }
    }

    KeReleaseSpinLock(&Lock);
    return NULL;
}
