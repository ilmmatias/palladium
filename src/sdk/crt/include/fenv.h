/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef FENV_H
#define FENV_H

#include <stdint.h>

#if __has_include(ARCH_MAKE_INCLUDE_PATH(crt_impl, fenv.h))
#include ARCH_MAKE_INCLUDE_PATH(crt_impl, fenv.h)
#else
#error "Undefined ARCH for the CRT module!"
#endif /* __has_include */

#define __STDC_VERSION_FENV_H__ 202311L

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Floating-point exceptions. */
int feclearexcept(int excepts);
int fegetexceptflag(fexcept_t *flagp, int excepts);
int feraiseexcept(int excepts);
int fesetexcept(int excepts);
int fesetexceptflag(const fexcept_t *flagp, int excepts);
int fetestexceptflag(const fexcept_t *flagp, int excepts);
int fetestexcept(int excepts);

/* Rounding and other control modes. */
int fegetmode(femode_t *modep);
int fegetround(void);
int fesetmode(const femode_t *modep);
int fesetround(int rnd);

/* Environment. */
int fegetenv(fenv_t *envp);
int feholdexcept(fenv_t *envp);
int fesetenv(const fenv_t *envp);
int feupdateenv(const fenv_t *envp);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FENV_H */
