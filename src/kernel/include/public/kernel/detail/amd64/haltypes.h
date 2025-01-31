/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_AMD64_HALTYPES_H_
#define _KERNEL_DETAIL_AMD64_HALTYPES_H_

#include <stdint.h>
#include <xmmintrin.h>

typedef struct __attribute__((packed)) {
    uint64_t Rax;
    uint64_t Rcx;
    uint64_t Rdx;
    uint64_t R8;
    uint64_t R9;
    uint64_t R10;
    uint64_t R11;
    uint64_t Padding0;
    __m128 Xmm0;
    __m128 Xmm1;
    __m128 Xmm2;
    __m128 Xmm3;
    __m128 Xmm4;
    __m128 Xmm5;
    uint64_t Padding1;
    uint64_t Mxcsr;
    uint64_t Irql;
    uint64_t FaultAddress;
    union {
        uint64_t ErrorCode;
        uint64_t InterruptNumber;
    };
    uint64_t Rip;
    uint64_t SegCs;
    uint64_t Rflags;
    uint64_t Rsp;
    uint64_t SegSs;
} HalInterruptFrame;

typedef struct __attribute__((packed)) {
    uint64_t Rbx;
    uint64_t Rbp;
    uint64_t Rsi;
    uint64_t Rdi;
    uint64_t R12;
    uint64_t R13;
    uint64_t R14;
    uint64_t R15;
    __m128 Xmm6;
    __m128 Xmm7;
    __m128 Xmm8;
    __m128 Xmm9;
    __m128 Xmm10;
    __m128 Xmm11;
    __m128 Xmm12;
    __m128 Xmm13;
    __m128 Xmm14;
    __m128 Xmm15;
    uint64_t Mxcsr;
    uint64_t ReturnAddress;
} HalExceptionFrame;

typedef struct __attribute__((packed)) {
    void (*EntryPoint)(void *);
    void *Parameter;
} HalStartFrame;

typedef struct {
    volatile uint64_t Busy;
    uint64_t Rsp;
} HalContextFrame;

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

#endif /* _KERNEL_DETAIL_AMD64_HALTYPES_H_ */
