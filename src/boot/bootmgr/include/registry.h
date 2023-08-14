/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _REGISTRY_
#define _REGISTRY_

#include <stdint.h>

#define REG_KEY_NONE 0x00
#define REG_KEY_STRING 0x01
#define REG_KEY_INTEGER 0x02
#define REG_KEY_BINARY 0x03
#define REG_SUBKEY 0x80

typedef struct RegKey {
    int Type;
    union {
        char *String;
        uint64_t Integer;
        struct {
            int Size;
            uint8_t *Data;
        } Binary;
        struct {
            char *Name;
            uint32_t EntryCount;
            struct RegKey *Entries;
        } SubKey;
    };
} RegKey;

#endif /* _REGISTRY_ */
