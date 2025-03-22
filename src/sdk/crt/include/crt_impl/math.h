/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef CRT_IMPL_MATH_H
#define CRT_IMPL_MATH_H

#include <errno.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function is used to raise a floating point exception while setting up the errno.
 *
 * PARAMETERS:
 *     except - Some value that will be evaluated to give the wanted exception.
 *     err - Which errno to save.
 *
 * RETURN VALUE:
 *     None
 *-----------------------------------------------------------------------------------------------*/
static inline void __raise_errnof(float except, int err) {
    __asm__ volatile("" : : "m"(except));
    /* TODO: Uncomment the code below.
     * errno = err; */
    (void)err;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function is used to raise a floating point exception while setting up the errno.
 *
 * PARAMETERS:
 *     except - Some value that will be evaluated to give the wanted exception.
 *     err - Which errno to save.
 *
 * RETURN VALUE:
 *     None
 *-----------------------------------------------------------------------------------------------*/
static inline void __raise_errno(double except, int err) {
    __asm__ volatile("" : : "m"(except));
    /* TODO: Uncomment the code below.
     * errno = err; */
    (void)err;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function is used to raise a floating point exception while setting up the errno.
 *
 * PARAMETERS:
 *     except - Some value that will be evaluated to give the wanted exception.
 *     err - Which errno to save.
 *
 * RETURN VALUE:
 *     None
 *-----------------------------------------------------------------------------------------------*/
static inline void __raise_errnol(long double except, int err) {
    __asm__ volatile("" : : "m"(except));
    /* TODO: Uncomment the code below.
     * errno = err; */
    (void)err;
}

/* Helper functions for sin/cos/tan. */
double __rem_pio2f(float x, int *q);
double __cosf(double x);
double __sinf(double x);
double __tanf(double x);
double __cotf(double x);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CRT_IMPL_MATH_H */
