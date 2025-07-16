/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _RT_AMD64_CONTEXT_H_
#define _RT_AMD64_CONTEXT_H_

#include <stdint.h>
#include <x86intrin.h>

typedef struct __attribute__((aligned(16))) {
    union {
        struct __attribute__((packed)) {
            uint64_t Rax;
            uint64_t Rcx;
            uint64_t Rdx;
            uint64_t Rbx;
            uint64_t Rsp;
            uint64_t Rbp;
            uint64_t Rsi;
            uint64_t Rdi;
            uint64_t R8;
            uint64_t R9;
            uint64_t R10;
            uint64_t R11;
            uint64_t R12;
            uint64_t R13;
            uint64_t R14;
            uint64_t R15;
        };
        uint64_t Gpr[16];
    };
    union {
        struct __attribute__((packed)) {
            __m128 Xmm0;
            __m128 Xmm1;
            __m128 Xmm2;
            __m128 Xmm3;
            __m128 Xmm4;
            __m128 Xmm5;
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
        };
        __m128 Xmm[16];
    };
    uint64_t Rip;
    uint64_t Rflags;
} RtContext;

#endif /* _RT_AMD64_EXCEPT_H_ */
