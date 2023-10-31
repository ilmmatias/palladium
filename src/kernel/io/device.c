/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <io.h>
#include <mm.h>
#include <stddef.h>
#include <string.h>

typedef struct DeviceListEntry {
    IoDevice Device;
    struct DeviceListEntry *Next;
} DeviceListEntry;

static DeviceListEntry *DeviceListHead = NULL;
static DeviceListEntry *DeviceListTail = NULL;

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

    DeviceListEntry *Entry = MmAllocateBlock(sizeof(DeviceListEntry));
    if (!Entry) {
        return 0;
    }

    Entry->Device.Name = MmAllocateBlock(strlen(Name) + 1);
    if (!Entry->Device.Name) {
        MmFreeBlock(Entry);
        return 0;
    }

    strcpy((char *)Entry->Device.Name, Name);
    Entry->Device.Read = Read;
    Entry->Device.Write = Write;
    Entry->Next = NULL;

    if (DeviceListTail) {
        DeviceListTail->Next = Entry;
    } else {
        DeviceListHead = Entry;
    }

    DeviceListTail = Entry;

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

    DeviceListEntry *Entry = DeviceListHead;
    while (Entry) {
        if (!strcmp(Entry->Device.Name, Name)) {
            return &Entry->Device;
        }

        Entry = Entry->Next;
    }

    return NULL;
}
