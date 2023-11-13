/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _INI_H_
#define _INI_H_

#include <rt.h>

#define BM_INI_STRING 0
#define BM_INI_ARRAY 1

typedef struct {
    RtSList ListHeader;
    char *Value;
} BmIniArray;

typedef struct {
    RtSList ListHeader;
    int Type;
    char *Name;
    union {
        char *StringValue;
        RtSList ArrayListHead;
    };
} BmIniValue;

typedef struct {
    RtSList ListHeader;
    char *Name;
    RtSList ValueListHead;
} BmIniSection;

typedef struct {
    RtSList SectionListHead;
    size_t Sections;
} BmIniHandle;

BmIniHandle *BmOpenIniFile(const char *Path);
void BmCloseIniFile(BmIniHandle *Handle);
BmIniValue *
BmGetIniValue(BmIniHandle *Handle, const char *SectionName, const char *ValueName, int Type);

#endif /* _INI_H_ */
