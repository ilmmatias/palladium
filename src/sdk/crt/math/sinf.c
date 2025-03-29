/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/ieee754.h>
#include <crt_impl/math.h>
#include <stddef.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements the core approximation of sin(x) between [-pi/4;pi/4].
 *
 * PARAMETERS:
 *     x - Which number to use as input.
 *
 * RETURN VALUE:
 *     Approximation of the result.
 *-----------------------------------------------------------------------------------------------*/
double __sinf(double x) {
    /* Coefficients generated using sollya:
     *     F = sin(x);
     *     G = (F(sqrt(x)) - sqrt(x)) / (x * sqrt(x));
     *     W = proc(P) { return P(x^2) * x^3 + x; };
     *     I = 3;
     *     T = [|D...|];
     *     RL = 1e-50;
     *     RR = pi/4;
     *     O = absolute;
     *     P = fpminimax(G, I, T, [RL;RR^2], O);
     *     C = horner(P);
     *     E = dirtyinfnorm(F(x) - W(P), [RL;RR]);
     * Maximum error: 1.36e-11. */
    double x2 = x * x;
    double x3 = x * x2;
    double res = 0x1.6dbe4ad0c24cep-19;
    res = res * x2 - 0x1.a013a88a3496p-13;
    res = res * x2 + 0x1.11110df0122fbp-7;
    res = res * x2 - 0x1.555555545e87dp-3;
    res = res * x3 + x;
    return res;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements an approximation of sin(x), suitable for any float value.
 *
 * PARAMETERS:
 *     x - Which number to use as input.
 *
 * RETURN VALUE:
 *     Approximation of the result.
 *-----------------------------------------------------------------------------------------------*/
float sinf(float x) {
    /* inf -> raise(INVALID) and return +-NaN
     * nan -> return +- NaN
     * We can share the same code path for these (just add an extra if for the raiseexcept). */
    uint32_t xi = __from_float(x);
    uint32_t xi_abs = xi & ~0x80000000;
    if (xi_abs >= 0x7f800000) {
        if (!(xi & 0x7fffff)) {
            __raise_errnof(0.0f / 0.0f, EDOM);
        }

        return x - x;
    }

    /* For very small values, the rounding errors inherant to float values make sin(x)=x a
     * properly rounded approximation. */
    if (xi_abs < 0x39e89769) {
        return x;
    }

    /* The core approx function works on the range [-pi/4;pi/4], if we're inside it, no need
     * to reduce the argument. */
    if (xi_abs < 0x3f490fdb) {
        return __sinf(x);
    }

    /* Otherwise, reduce and use the quadrant to choose the right operation to execute. */
    int q;
    double res = __rem_pio2f(x, &q);
    switch (q & 3) {
        case 0:
            return __sinf(res);
        case 1:
            return __cosf(res);
        case 2:
            return -__sinf(res);
        case 3:
            return -__cosf(res);
        default:
            unreachable();
    }
}
