/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _REGISTRY_H_
#define _REGISTRY_H_

#include <stdint.h>
#include <stdio.h>

#define REG_FILE_SIGNATURE "REFF"
#define REG_BLOCK_SIGNATURE "REGB"

#define REG_BLOCK_SIZE 1024
#define REG_NAME_SIZE 32

#define REG_ENTRY_REMOVED 0x00
#define REG_ENTRY_BYTE 0x01
#define REG_ENTRY_WORD 0x02
#define REG_ENTRY_DWORD 0x03
#define REG_ENTRY_QWORD 0x04
#define REG_ENTRY_STRING 0x05
#define REG_ENTRY_BINARY 0x06
#define REG_ENTRY_KEY 0x80

typedef struct __attribute__((packed)) {
    char Signature[4];
    char Reserved[12];
} RegFileHeader;

typedef struct __attribute__((packed)) {
    char Signature[4];
    uint32_t InsertOffsetHint;
    uint32_t OffsetToNextBlock;
} RegBlockHeader;

typedef struct __attribute__((packed)) {
    uint8_t Type;
    uint16_t Length;
    uint32_t NameHash;
} RegEntryHeader;

typedef struct {
    char Buffer[REG_BLOCK_SIZE];
    FILE *Stream;
} RegHandle;

extern RegHandle *BmBootRegistry;

void BmInitRegistry(void);
RegHandle *BmLoadRegistry(const char *Path);
RegEntryHeader *BmFindRegistryEntry(RegHandle *Handle, RegEntryHeader *Parent, const char *Path);
RegEntryHeader *BmGetRegistryEntry(RegHandle *Handle, RegEntryHeader *Parent, int Which);

#endif /* _REGISTRY_H_ */
