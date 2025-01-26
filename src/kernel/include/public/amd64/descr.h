/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _AMD64_DESCR_H_
#define _AMD64_DESCR_H_

#include <stdint.h>

#define DESCR_SEG_NULL 0x00
#define DESCR_SEG_KCODE 0x08
#define DESCR_SEG_KDATA 0x10
#define DESCR_SEG_UCODE 0x18
#define DESCR_SEG_UDATA 0x20
#define DESCR_SEG_TSS 0x28

#define DESCR_DPL_KERNEL 0x00
#define DESCR_DPL_USER 0x03

#define GDT_TYPE_TSS 0x09
#define GDT_TYPE_CODE 0x1A
#define GDT_TYPE_DATA 0x12

#define IDT_TYPE_INT 0x0E
#define IDT_TYPE_TRAP 0x0F

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
        uint16_t LimitLow;
        uint16_t BaseLow;
        uint8_t BaseMiddle;
        struct __attribute__((packed)) {
            uint16_t Type : 5;
            uint16_t Dpl : 2;
            uint16_t Present : 1;
            uint16_t LimitHigh : 4;
            uint16_t System : 1;
            uint16_t LongMode : 1;
            uint16_t DefaultBig : 1;
            uint16_t Granularity : 1;
        };
        uint8_t BaseHigh;
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

typedef union {
    struct __attribute__((packed)) {
        uint16_t BaseLow;
        uint16_t Segment;
        struct __attribute__((packed)) {
            uint16_t IstIndex : 3;
            uint16_t Reserved0 : 5;
            uint16_t Type : 4;
            uint16_t Reserved1 : 1;
            uint16_t Dpl : 2;
            uint16_t Present : 1;
        };
        uint16_t BaseMiddle;
        uint32_t BaseHigh;
        uint32_t Reserved2;
    };
    struct __attribute__((packed)) {
        uint64_t DataLow;
        uint64_t DataHigh;
    };
} HalpIdtEntry;

typedef struct __attribute__((packed)) {
    uint16_t Limit;
    uint64_t Base;
} HalpIdtDescriptor;

#endif /* _AMD64_DESCR_H_ */
