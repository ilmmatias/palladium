/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _FILE_H_
#define _FILE_H_

#include <rt/list.h>

struct BmFile;
typedef struct BmFile BmFile;

typedef BmFile *(*BmFileOpenRootFn)(void *Context);
typedef void (*BmFileCloseFn)(void *Context);
typedef int (*BmFileReadFn)(void *Context, uint64_t Offset, uint64_t Size, void *Buffer);
typedef BmFile *(*BmFileReadEntryFn)(void *Context, const char *Name);
typedef char *(*BmFileIterateFn)(void *Context, int Index);

typedef struct {
    RtSList ListHeader;
    int Index;
    int Active;
    uint64_t Offset;
    void *DeviceContext;
    BmFileReadFn ReadDisk;
    void *FsContext;
    BmFileOpenRootFn OpenRoot;
} BmPartition;

typedef struct BmFile {
    uint64_t Size;
    void *Context;
    BmFileCloseFn Close;
    BmFileReadFn Read;
    BmFileReadEntryFn ReadEntry;
    BmFileIterateFn Iterate;
} BmFile;

void BiInitializeDisks(void *BootInfo);

void BiProbeDisk(RtSList *ListHead, BmFileReadFn ReadDisk, void *Context, uint64_t SectorSize);
void BiProbeMbrDisk(RtSList *ListHead, BmFileReadFn ReadDisk, void *Context, uint64_t SectorSize);

int BiProbePartition(BmPartition *Partition, BmFileReadFn ReadPartition);
int BiProbeExfat(BmPartition *Partition, BmFileReadFn ReadPartition);
int BiProbeFat32(BmPartition *Partition, BmFileReadFn ReadPartition);
int BiProbeIso9660(BmPartition *Partition, BmFileReadFn ReadPartition);
int BiProbeNtfs(BmPartition *Partition, BmFileReadFn ReadPartition);

BmFile *BiOpenDevice(char **Name, RtSList **ListHead);
BmFile *BiOpenPartition(RtSList *ListHead, const char *Name);
BmFile *BiOpenBootPartition(void);
BmFile *BiOpenRoot(BmPartition *Partition);

BmFile *BmOpenFile(const char *Path);
void BmCloseFile(BmFile *File);
int BmReadFile(BmFile *File, uint64_t Offset, uint64_t Size, void *Buffer);
BmFile *BmReadDirectoryEntry(BmFile *Directory, const char *Name);
char *BmIterateDirectory(BmFile *Directory, int Index);

#endif /* _FILE_H_ */
