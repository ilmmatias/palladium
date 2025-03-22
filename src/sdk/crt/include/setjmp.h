/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef SETJMP_H
#define SETJMP_H

#if defined(ARCH_amd64)
#include <crt_impl/amd64/setjmp.h>
#else
#error "Undefined ARCH for the CRT module!"
#endif /* ARCH */

#define __STDC_VERSION_SETJMP_H__ 202311L

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Save calling environment. */
int setjmp(jmp_buf env);

/* Restore calling environment. */
[[noreturn]] void longjmp(jmp_buf env, int val);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SETJMP_H */
