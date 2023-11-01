/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <io.h>
#include <mm.h>
#include <rt.h>
#include <string.h>

static RtSinglyLinkedListEntry DeviceListHead = {.Next = NULL};

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
    if (!Name || !Read || !Write || IoOpenDevice(Name)) {
        return 0;
    }

    IoDevice *Entry = MmAllocatePool(sizeof(IoDevice), "IOP ");
    if (!Entry) {
        return 0;
    }

    Entry->Name = MmAllocatePool(strlen(Name) + 1, "IOP ");
    if (!Entry->Name) {
        MmFreePool(Entry, "IOP ");
        return 0;
    }

    strcpy((char *)Entry->Name, Name);
    Entry->Read = Read;
    Entry->Write = Write;

    RtPushSinglyLinkedList(&DeviceListHead, &Entry->ListHeader);
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
    if (!Name) {
        return NULL;
    }

    for (RtSinglyLinkedListEntry *Entry = DeviceListHead.Next; Entry; Entry = Entry->Next) {
        IoDevice *Device = CONTAINING_RECORD(Entry, IoDevice, ListHeader);
        if (!strcmp(Device->Name, Name)) {
            return Device;
        }
    }

    return NULL;
}
