/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _FILE_H_
#define _FILE_H_

#include <stddef.h>
#include <stdint.h>

#define FILE_TYPE_NONE 0
#define FILE_TYPE_ARCH 1
#define FILE_TYPE_EXFAT 2
#define FILE_TYPE_NTFS 3

typedef struct {
    int Type;
    void *PrivateData;
} FileContext;

int BiCopyFileContext(FileContext *Context, FileContext *Copy);
int BiReadDirectoryEntry(FileContext *Context, const char *Name);

int BiCopyArchDevice(FileContext *Context, FileContext *Copy);
int BiOpenArchDevice(const char *Segment, FileContext *Context);
void BiFreeArchDevice(FileContext *Context);
int BiReadArchDirectoryEntry(FileContext *Context, const char *Name);
int BiReadArchDevice(FileContext *Context, void *Buffer, size_t Start, size_t Size, size_t *Read);

int BiCopyExfat(FileContext *Context, FileContext *Copy);
int BiProbeExfat(FileContext *Context);
void BiCleanupExfat(FileContext *Context);
int BiTraverseExfatDirectory(FileContext *Context, const char *Name);
int BiReadExfatFile(FileContext *Context, void *Buffer, size_t Start, size_t Size, size_t *Read);

int BiCopyNtfs(FileContext *Context, FileContext *Copy);
int BiProbeNtfs(FileContext *Context);
void BiCleanupNtfs(FileContext *Context);
int BiTraverseNtfsDirectory(FileContext *Context, const char *Name);
int BiReadNtfsFile(FileContext *Context, void *Buffer, size_t Start, size_t Size, size_t *Read);

#endif /* _FILE_H_ */
