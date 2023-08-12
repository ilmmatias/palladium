/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _X86_BIOS_H_
#define _X86_BIOS_H_

#include <stddef.h>
#include <stdint.h>

#define BIOS_MEMORY_REGION_TYPE_AVAILABLE 1
#define BIOS_MEMORY_REGION_TYPE_USED 0x1000

typedef struct __attribute__((packed)) {
    uint8_t BootDrive;
    uint32_t MemoryCount;
    uint32_t MemoryRegions;
} BiosBootBlock;

typedef struct __attribute__((packed)) {
    uint64_t BaseAddress;
    uint64_t Length;
    uint32_t Type;
} BiosMemoryRegion;

typedef struct __attribute__((packed)) {
    uint32_t Edi, Esi, Ebp, Esp, Ebx, Edx, Ecx, Eax, Eflags;
} BiosRegisters;

void BiosCall(uint8_t Number, BiosRegisters *Registers);
void BiosDetectDisks(BiosBootBlock *Data);

#endif /* _X86_BIOS_H_ */
