/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef CRT_IMPL_IEEE754_H
#define CRT_IMPL_IEEE754_H

#include <stdint.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function casts an IEEE754 single-precision format floating point number into its
 *     underlying representation.
 *
 * PARAMETERS:
 *     x - Which value to convert.
 *
 * RETURN VALUE:
 *     Raw bits of the IEEE754 float.
 *-----------------------------------------------------------------------------------------------*/
static inline uint32_t __from_float(float x) {
    return (union {
               float f;
               uint32_t i;
           }){x}
        .i;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function casts the raw bits of an IEEE754 single-precision format floating point number
 *     into a machine floating point number.
 *
 * PARAMETERS:
 *     x - Which value to convert.
 *
 * RETURN VALUE:
 *     Floating point number.
 *-----------------------------------------------------------------------------------------------*/
static inline float __as_float(uint32_t x) {
    return (union {
               uint32_t i;
               float f;
           }){x}
        .f;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function casts an IEEE754 double-precision format floating point number into its
 *     underlying representation.
 *
 * PARAMETERS:
 *     x - Which value to convert.
 *
 * RETURN VALUE:
 *     Raw bits of the IEEE754 double.
 *-----------------------------------------------------------------------------------------------*/
static inline uint64_t __from_double(double x) {
    return (union {
               double f;
               uint64_t i;
           }){x}
        .i;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function casts the raw bits of an IEEE754 double-precision format floating point number
 *     into a machine floating point number.
 *
 * PARAMETERS:
 *     x - Which value to convert.
 *
 * RETURN VALUE:
 *     Floating point number.
 *-----------------------------------------------------------------------------------------------*/
static inline double __as_double(uint64_t x) {
    return (union {
               uint64_t i;
               double f;
           }){x}
        .f;
}

#endif /* CRT_IMPL_IEEE754_H */
