/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <rt.h>

#define BM_ENTRY_PALLADIUM 0
#define BM_ENTRY_CHAINLOAD 1

typedef struct {
    int Type;
    const char *Text;
    char *Icon;
    union {
        struct {
            const char *SystemFolder;
            RtSList *DriverListHead;
        } Palladium;
        struct {
            const char *Path;
        } Chainload;
    };
} BmMenuEntry;

void BiLoadConfig(void);

int BmGetDefaultTimeout(void);
int BmGetDefaultSelectionIndex(void);
int BmGetMenuEntryCount(void);
void BmGetMenuEntry(int Index, BmMenuEntry *Entry);

#endif /* _CONFIG_H_ */
