/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_KDFUNCS_H_
#define _KERNEL_DETAIL_KDFUNCS_H_

#include <stdarg.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, kdfuncs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, kdfuncs.h)
#endif /* __has__include */
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void KdPrintVariadic(int Type, const char *Message, va_list Arguments);
__attribute__((format(printf, 2, 3))) void KdPrint(int Type, const char *Message, ...);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _KERNEL_DETAIL_KDFUNCS_H_ */
