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
    size_t PrivateSize;
    void *PrivateData;
    uint64_t FileLength;
} FileContext;

FileContext *BmOpenFile(const char *Path);
void BmCloseFile(FileContext *Context);
int BmReadFile(FileContext *Context, void *Buffer, size_t Start, size_t Size, size_t *Read);

int BiCopyFileContext(FileContext *Context, FileContext *Copy);
int BiReadDirectoryEntry(FileContext *Context, const char *Name);

int BiOpenConsoleDevice(const char *Segment, FileContext *Context);
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

int BiOpenArchDevice(const char *Segment, FileContext *Context);
int BiReadArchDirectoryEntry(FileContext *Context, const char *Name);
int BiReadArchDevice(FileContext *Context, void *Buffer, size_t Start, size_t Size, size_t *Read);

int BiProbeExfat(FileContext *Context);
int BiTraverseExfatDirectory(FileContext *Context, const char *Name);
int BiReadExfatFile(FileContext *Context, void *Buffer, size_t Start, size_t Size, size_t *Read);

int BiProbeFat32(FileContext *Context);
int BiTraverseFat32Directory(FileContext *Context, const char *Name);
int BiReadFat32File(FileContext *Context, void *Buffer, size_t Start, size_t Size, size_t *Read);

int BiProbeIso9660(FileContext *Context, uint16_t BytesPerSector);
int BiTraverseIso9660Directory(FileContext *Context, const char *Name);
int BiReadIso9660File(FileContext *Context, void *Buffer, size_t Start, size_t Size, size_t *Read);

int BiProbeNtfs(FileContext *Context);
int BiTraverseNtfsDirectory(FileContext *Context, const char *Name);
int BiReadNtfsFile(FileContext *Context, void *Buffer, size_t Start, size_t Size, size_t *Read);

#endif /* _FILE_H_ */
