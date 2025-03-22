/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef CRT_IMPL_AMD64_FENV_H
#define CRT_IMPL_AMD64_FENV_H

#include <stdint.h>

#define FE_INVALID 0x01
#define FE_DENORMAL 0x02
#define FE_DIVBYZERO 0x04
#define FE_OVERFLOW 0x08
#define FE_UNDERFLOW 0x10
#define FE_INEXACT 0x20
#define FE_ALL_EXCEPT \
    (FE_INVALID | FE_DENORMAL | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW | FE_INEXACT)

#define FE_DFL_MODE ((const femode_t*)-1L)
#define FE_DFL_ENV ((const fenv_t*)-1L)

typedef uint64_t fexcept_t;

typedef struct {
    uint8_t fpu_env[28];
    uint64_t mxcsr;
} fenv_t;

typedef struct {
    uint16_t fpucw;
    uint64_t mxcsr;
} femode_t;

#endif /* CRT_IMPL_AMD64_FENV_H */
