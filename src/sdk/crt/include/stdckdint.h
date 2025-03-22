/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef STDCKDINT_H
#define STDCKDINT_H

#define __STDC_VERSION_STDCKDINT_H__ 202311L

/* Checked Integer Operation Type-generic Macros. */
#define ckd_add(result, a, b) __builtin_add_overflow(a, b, result)
#define ckd_sub(result, a, b) __builtin_sub_overflow(a, b, result)
#define ckd_mul(result, a, b) __builtin_mul_overflow(a, b, result)

#endif /* STDCKDINT_H */
