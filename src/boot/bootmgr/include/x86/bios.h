/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _X86_BIOS_H_
#define _X86_BIOS_H_

#include <stddef.h>
#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint32_t Edi, Esi, Ebp, Esp, Ebx, Edx, Ecx, Eax, Eflags;
} BiosRegisters;

void BiosCall(uint8_t Number, BiosRegisters *Registers);

#endif /* _X86_BIOS_H_ */
