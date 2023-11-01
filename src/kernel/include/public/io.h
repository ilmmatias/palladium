/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _IO_H_
#define _IO_H_

#include <rt.h>

struct IoDevice;
typedef struct IoDevice IoDevice;

typedef uint64_t (*IoReadFn)(IoDevice *Device, void *Buffer, uint64_t Offset, uint64_t Size);
typedef uint64_t (*IoWriteFn)(IoDevice *Device, const void *Buffer, uint64_t Offset, uint64_t Size);

struct IoDevice {
    RtSinglyLinkedListEntry ListEntry;
    const char *Name;
    IoReadFn Read;
    IoWriteFn Write;
};

int IoCreateDevice(const char *Name, IoReadFn Read, IoWriteFn Write);
IoDevice *IoOpenDevice(const char *Name);

#endif /* _IO_H_ */
