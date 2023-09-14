/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _X86_IDT_H_
#define _X86_IDT_H_

#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint16_t OffsetLow;
    uint16_t Segment;
    uint8_t Ist;
    uint8_t TypeAttributes;
    uint16_t OffsetMid;
    uint32_t OffsetHigh;
    uint32_t Reserved;
} IdtDescriptor;

extern IdtDescriptor IdtDescs[256];
extern uint32_t IrqStubTable[33];

#endif /* _X86_IDT_H_ */
