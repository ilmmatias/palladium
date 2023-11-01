/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _MI_H_
#define _MI_H_

#include <mm.h>

typedef struct MiPageEntry {
    uint8_t References;
    uint64_t GroupBase;
    uint32_t GroupPages;
    struct MiPageEntry *NextGroup;
    struct MiPageEntry *PreviousGroup;
} MiPageEntry;

void MiInitializePageAllocator(void *LoaderData);

#endif /* _MI_H_ */
