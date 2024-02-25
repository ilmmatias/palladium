/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _IO_H_
#define _IO_H_

#include <rt/list.h>

struct IoDevice;
typedef struct IoDevice IoDevice;

typedef uint64_t (*IoReadFn)(IoDevice *Device, void *Buffer, uint64_t Offset, uint64_t Size);
typedef uint64_t (*IoWriteFn)(IoDevice *Device, const void *Buffer, uint64_t Offset, uint64_t Size);

struct IoDevice {
    RtSList ListHeader;
    const char *Name;
    IoReadFn Read;
    IoWriteFn Write;
};

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int IoCreateDevice(const char *Name, IoReadFn Read, IoWriteFn Write);
IoDevice *IoOpenDevice(const char *Name);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _IO_H_ */
