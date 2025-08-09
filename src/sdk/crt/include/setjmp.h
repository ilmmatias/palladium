/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef SETJMP_H
#define SETJMP_H

#include <crt_impl/common.h>

#if __has_include(ARCH_MAKE_INCLUDE_PATH(crt_impl, setjmp.h))
#include ARCH_MAKE_INCLUDE_PATH(crt_impl, setjmp.h)
#else
#error "Undefined ARCH for the CRT module!"
#endif /* __has_include */

#define __STDC_VERSION_SETJMP_H__ 202311L

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Save calling environment. */
int setjmp(jmp_buf env);

/* Restore calling environment. */
CRT_NORETURN void longjmp(jmp_buf env, int val);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SETJMP_H */
