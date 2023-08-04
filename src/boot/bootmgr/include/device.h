/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _DEVICE_H_
#define _DEVICE_H_

#include <stddef.h>
#include <stdint.h>

#define DEVICE_TYPE_NONE 0
#define DEVICE_TYPE_ARCH 1
#define DEVICE_TYPE_EXFAT 2

typedef struct {
    int Type;
    void *PrivateData;
} DeviceContext;

int BiOpenArchDevice(const char *Segment, DeviceContext *Context);
void BiFreeArchDevice(DeviceContext *Context);
int BiReadArchDevice(DeviceContext *Context, void *Buffer, uint64_t Start, size_t Size);
int BiReadArchDirectoryEntry(DeviceContext *Context, const char *Name);

void BiFreeDevice(DeviceContext *Context);
int BiReadDevice(DeviceContext *Context, void *Buffer, uint64_t Start, size_t Size);
int BiReadDirectoryEntry(DeviceContext *Context, const char *Name);

int BiProbeExfat(DeviceContext *Context);
void BiCleanupExfat(DeviceContext *Context);
int BiReadExfatFile(DeviceContext *Context, void *Buffer, uint64_t Start, size_t Size);
int BiTraverseExfatDirectory(DeviceContext *Context, const char *Name);

#endif /* _DEVICE_H_ */
