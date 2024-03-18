/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _RT_AMD64_CONTEXT_H_
#define _RT_AMD64_CONTEXT_H_

#include <stdint.h>

typedef struct {
    union {
        struct {
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

    uint64_t Rip;
    uint64_t Rflags;
} RtContext;

#endif /* _RT_AMD64_EXCEPT_H_ */
