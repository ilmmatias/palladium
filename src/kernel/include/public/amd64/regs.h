/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _AMD64_REGS_H_
#define _AMD64_REGS_H_

#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint64_t R15;
    uint64_t R14;
    uint64_t R13;
    uint64_t R12;
    uint64_t R11;
    uint64_t R10;
    uint64_t R9;
    uint64_t R8;
    uint64_t Rdi;
    uint64_t Rsi;
    uint64_t Rbp;
    uint64_t Rbx;
    uint64_t Rdx;
    uint64_t Rcx;
    uint64_t Rax;
    uint64_t InterruptNumber;
    uint64_t ErrorCode;
    uint64_t Rip;
    uint64_t Cs;
    uint64_t Rflags;
    uint64_t Rsp;
    uint64_t Ss;
} HalRegisterState;

#endif /* _AMD64_REGS_H_ */
