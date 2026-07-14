/* SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

/* IWYU pragma: private, include <setjmp.h> */

#ifndef CRT_IMPL_AMD64_SETJMP_H
#define CRT_IMPL_AMD64_SETJMP_H

#include <stdint.h>

typedef struct {
    uint64_t regs[10];
} jmp_buf;

#endif /* CRT_IMPL_AMD64_SETJMP_H */
