/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/ieee754.h>

/* Constants for the fast path. */
#define ROUND_FACTOR 0x1.8p52
#define INV_PIO2 0x1.45f306dc9c883p-1
#define PIO2_SCALED 0x1.921fb54442d18p-62

/* Contants for the slow path (+chosing when to use each). */
#define PIO2_HIGH 0x1.921fb544p0
#define PIO2_LOW 0x1.0b4611a6p-34
#define REM_PIO2_THRESHOLD 0x4aa562ae

/* 192 bits of 2/pi for the Payne-Hanek reduction in rem_pio2f_slow. */
static const uint32_t inv_pio2[] = {
    0xa2,       0xa2f9,     0xa2f983,   0xa2f9836e, 0xf9836e4e, 0x836e4e44, 0x6e4e4415, 0x4e441529,
    0x441529fc, 0x1529fc27, 0x29fc2757, 0xfc2757d1, 0x2757d1f5, 0x57d1f534, 0xd1f534dd, 0xf534ddc0,
    0x34ddc0db, 0xddc0db62, 0xc0db6295, 0xdb629599, 0x6295993c, 0x95993c43, 0x993c4390, 0x3c439041,
};

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements Cody-Waite reduction (using 2 coefficents) that can be used for
 *     smaller numbers.
 *
 * PARAMETERS:
 *     x - Which number to reduce.
 *     q - Output; Which quadrant the number is in.
 *
 * RETURN VALUE:
 *     Value reduced to between -pi/4 and pi/4.
 *-----------------------------------------------------------------------------------------------*/
static double rem_pio2f_fast(float x, int *q) {
    double ipart = x * INV_PIO2 + ROUND_FACTOR;
    ipart -= ROUND_FACTOR;
    double res = x - ipart * PIO2_HIGH;
    res -= ipart * PIO2_LOW;
    *q = ipart;
    return res;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements the slower Payne-Hanek algorithm for use with bigger numbers.
 *
 * PARAMETERS:
 *     xi - Raw bits of the number we want to reduce. Needs to be at least 2.0f.
 *     q - Output; Which quadrant the number is in.
 *
 * RETURN VALUE:
 *     Value reduced to between -pi/4 and pi/4.
 *-----------------------------------------------------------------------------------------------*/
static double rem_pio2f_slow(uint32_t xi, int *q) {
    /* Extract the exponent, using the low bits to calculate how we should adjust the mantissa, and
     * the high bits to calculate the entry point into the 2/pi table. */
    int index = (xi >> 26) & 15;
    int shift = (xi >> 23) & 7;

    /* Extract the mantissa (+its implicit 23rd bit), and adjust it according to the exponent. */
    uint32_t mant = ((xi & 0x7fffff) | 0x800000) << shift;

    /* Extract 96-bits of 2/pi from the table. */
    uint32_t coeff_high = inv_pio2[index];
    uint32_t coeff_mid = inv_pio2[index + 4];
    uint32_t coeff_low = inv_pio2[index + 8];

    /* Calculate the three required 2.62 products, and combine them. */
    uint64_t prod_high = (uint64_t)mant * coeff_high;
    uint64_t prod_mid = (uint64_t)mant * coeff_mid;
    uint64_t prod_low = (uint64_t)mant * coeff_low;
    uint64_t prod = (prod_high << 32) + prod_mid + (prod_low >> 32);

    /* Round to nearest. */
    int ipart = (prod + 0x2000000000000000) >> 62;
    uint64_t fpart = prod - ((uint64_t)ipart << 62);

    /* Finally, convert the fraction to an integer, and scale it back by pi/2. */
    double res = (int64_t)fpart * PIO2_SCALED;
    int sign = 1 - 2 * (xi >> 31);
    res *= sign;
    *q = ipart * sign;
    return res;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reduces a number for usage by the internal __sin/cos/tanf functions.
 *
 * PARAMETERS:
 *     x - Which number to reduce.
 *     q - Output; Which quadrant the number is in.
 *
 * RETURN VALUE:
 *     Value reduced to between -pi/4 and pi/4.
 *-----------------------------------------------------------------------------------------------*/
double __rem_pio2f(float x, int *q) {
    uint32_t xi = __from_float(x);
    if ((xi & ~0x80000000) < REM_PIO2_THRESHOLD) {
        return rem_pio2f_fast(x, q);
    } else {
        return rem_pio2f_slow(xi, q);
    }
}
