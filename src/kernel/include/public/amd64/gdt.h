/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _AMD64_GDT_H_
#define _AMD64_GDT_H_

#include <stdint.h>

#define GDT_ENTRY_NULL 0x00
#define GDT_ENTRY_KCODE 0x08
#define GDT_ENTRY_KDATA 0x10
#define GDT_ENTRY_UCODE 0x18
#define GDT_ENTRY_UDATA 0x20
#define GDT_ENTRY_TSS 0x28

#define GDT_TYPE_TSS 0x09
#define GDT_TYPE_CODE 0x1A
#define GDT_TYPE_DATA 0x12

#define GDT_DPL_KERNEL 0x00
#define GDT_DPL_USER 0x03

typedef struct __attribute__((packed)) {
    uint32_t Reserved0;
    uint64_t Rsp0;
    uint64_t Rsp1;
    uint64_t Rsp2;
    uint64_t Ist[8];
    uint64_t Reserved1;
    uint16_t Reserved2;
    uint16_t IoMapBase;
} HalpTssEntry;

typedef union {
    struct __attribute__((packed)) {
        struct __attribute__((packed)) {
            uint64_t LimitLow : 16;
            uint64_t BaseLow : 24;
            uint64_t Type : 5;
            uint64_t Dpl : 2;
            uint64_t Present : 1;
            uint64_t LimitHigh : 4;
            uint64_t System : 1;
            uint64_t LongMode : 1;
            uint64_t DefaultBig : 1;
            uint64_t Granularity : 1;
            uint64_t BaseHigh : 8;
        };
        uint32_t BaseUpper;
        uint32_t MustBeZero;
    };
    struct __attribute__((packed)) {
        uint64_t DataLow;
        uint64_t DataHigh;
    };
} HalpGdtEntry;

typedef struct __attribute__((packed)) {
    uint16_t Limit;
    uint64_t Base;
} HalpGdtDescriptor;

#endif /* _AMD64_GDT_H_ */
