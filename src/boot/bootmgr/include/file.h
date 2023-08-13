/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _FILE_H_
#define _FILE_H_

#include <stddef.h>
#include <stdint.h>

#define FILE_TYPE_NONE 0
#define FILE_TYPE_CONSOLE 1
#define FILE_TYPE_ARCH 2
#define FILE_TYPE_EXFAT 3
#define FILE_TYPE_FAT32 4
#define FILE_TYPE_ISO9660 5
#define FILE_TYPE_NTFS 6

typedef struct {
    int Type;
    void *PrivateData;
    uint64_t FileLength;
} FileContext;

int BiCopyFileContext(FileContext *Context, FileContext *Copy);
int BiReadDirectoryEntry(FileContext *Context, const char *Name);

int BiOpenConsoleDevice(const char *Segment, FileContext *Context);
void BiFreeConsoleDevice(FileContext *Context);
int BiReadConsoleDevice(
    FileContext *Context,
    void *Buffer,
    size_t Start,
    size_t Size,
    size_t *Read);
int BiWriteConsoleDevice(
    FileContext *Context,
    const void *Buffer,
    size_t Start,
    size_t Size,
    size_t *Wrote);

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

int BiCopyFat32(FileContext *Context, FileContext *Copy);
int BiProbeFat32(FileContext *Context);
void BiCleanupFat32(FileContext *Context);
int BiTraverseFat32Directory(FileContext *Context, const char *Name);
int BiReadFat32File(FileContext *Context, void *Buffer, size_t Start, size_t Size, size_t *Read);

int BiCopyIso9660(FileContext *Context, FileContext *Copy);
int BiProbeIso9660(FileContext *Context, uint16_t BytesPerSector);
void BiCleanupIso9660(FileContext *Context);
int BiTraverseIso9660Directory(FileContext *Context, const char *Name);
int BiReadIso9660File(FileContext *Context, void *Buffer, size_t Start, size_t Size, size_t *Read);

int BiCopyNtfs(FileContext *Context, FileContext *Copy);
int BiProbeNtfs(FileContext *Context);
void BiCleanupNtfs(FileContext *Context);
int BiTraverseNtfsDirectory(FileContext *Context, const char *Name);
int BiReadNtfsFile(FileContext *Context, void *Buffer, size_t Start, size_t Size, size_t *Read);

#endif /* _FILE_H_ */
