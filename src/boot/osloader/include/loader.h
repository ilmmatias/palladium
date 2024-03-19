/* SPDX-FileCopyrightText: (C) 2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _LOADER_H_
#define _LOADER_H_

#include <rt/list.h>

typedef struct {
    const char *Name;
    uint64_t Address;
} OslpExportEntry;

typedef struct {
    RtDList ListHeader;
    void *PhysicalAddress;
    void *VirtualAddress;
    uint64_t BaseDiff;
    uint64_t ImageSize;
    void *EntryPoint;
    int *PageFlags;
    const char *Name;
    size_t ExportTableSize;
    OslpExportEntry *ExportTable;
} OslpLoadedProgram;

typedef struct {
    RtDList ListHeader;
    void *ImageBase;
    void *EntryPoint;
    uint32_t SizeOfImage;
    const char *ImageName;
} OslpModuleEntry;

int OslLoadExecutable(RtDList *LoadedPrograms, const char *ImageName, const char *ImagePath);
int OslFixupImports(RtDList *LoadedPrograms);
void OslFixupRelocations(RtDList *LoadedPrograms);
void OslCreateKernelModuleList(RtDList *LoadedPrograms, RtDList *ModuleListHead);

#endif /* _LOADER_H_ */
