/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _MI_H_
#define _MI_H_

#include <boot.h>
#include <mm.h>

#ifdef ARCH_amd64
#define MI_POOL_START 0xFFFF908000000000
#define MI_POOL_SIZE 0x2000000000
#else
#error "Undefined ARCH for the kernel module!"
#endif /* ARCH */

#define MI_MAP_WRITE 0x01
#define MI_MAP_EXEC 0x02

typedef struct __attribute__((packed)) MiPageEntry {
    uint8_t References;
    uint64_t GroupBase;
    uint32_t GroupPages;
    struct MiPageEntry *NextGroup;
    struct MiPageEntry *PreviousGroup;
} MiPageEntry;

void MiInitializePageAllocator(LoaderBootData *BootData);
void MiInitializePool(LoaderBootData *BootData);

#endif /* _MI_H_ */
