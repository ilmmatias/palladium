/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/ieee754.h>
#include <crt_impl/math.h>
#include <stddef.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements the core approximation of tan(x) between [-pi/4;pi/4].
 *
 * PARAMETERS:
 *     x - Which number to use as input.
 *
 * RETURN VALUE:
 *     Approximation of the result.
 *-----------------------------------------------------------------------------------------------*/
double __tanf(double x) {
    /* Coefficients generated using sollya:
     *     F = tan(x);
     *     G = (F(sqrt(x)) - sqrt(x)) / (x * sqrt(x));
     *     W = proc(P) { return P(x^2) * x^3 + x; };
     *     I = 6;
     *     T = [|D...|];
     *     RL = 1e-50;
     *     RR = pi/4;
     *     O = absolute;
     *     P = fpminimax(G, I, T, [RL;RR^2], O);
     *     C = horner(P);
     *     E = dirtyinfnorm(F(x) - W(P), [RL;RR]);
     * Maximum error: 3.63e-9. */
    double x2 = x * x;
    double x3 = x * x2;
    double res = 0x1.fcf62f4b7bd7fp-9;
    res = res * x2 + 0x1.23699a9a5db71p-10;
    res = res * x2 + 0x1.481bdc983da89p-7;
    res = res * x2 + 0x1.61f98c24e7d02p-6;
    res = res * x2 + 0x1.ba577d6f4319dp-5;
    res = res * x2 + 0x1.111077374721cp-3;
    res = res * x2 + 0x1.555555d62c348p-2;
    res = res * x3 + x;
    return res;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements the core approximation of 1/tan(x) between [-pi/4;pi/4].
 *
 * PARAMETERS:
 *     x - Which number to use as input.
 *
 * RETURN VALUE:
 *     Approximation of the result.
 *-----------------------------------------------------------------------------------------------*/
double __cotf(double x) {
    /* Coefficients generated using sollya:
     *    F = 1/tan(x);
     *    G = (F(sqrt(x)) - 1/sqrt(x)) / sqrt(x);
     *    W = proc(P) { return P(x^2) * x + 1/x; };
     *    I = 4;
     *    T = [|D...|];
     *    RL = 1e-50;
     *    RR = pi/4;
     *    O = absolute;
     *    P = fpminimax(G, I, T, [RL;RR^2], O);
     *    C = horner(P);
     *    E = dirtyinfnorm(F(x) - W(P), [RL;RR]);
     * Maximum error: 3.59e-10. */
    double x2 = x * x;
    double inv_x = 1.0 / x;
    double res = -0x1.a514da06f9c31p-16;
    res = res * x2 - 0x1.b778eee3bd453p-13;
    res = res * x2 - 0x1.15766295b5943p-9;
    res = res * x2 - 0x1.6c169a1725b7bp-6;
    res = res * x2 - 0x1.5555555d32589p-2;
    res = res * x + inv_x;
    return res;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements an approximation of tan(x), suitable for any float value.
 *
 * PARAMETERS:
 *     x - Which number to use as input.
 *
 * RETURN VALUE:
 *     Approximation of the result.
 *-----------------------------------------------------------------------------------------------*/
float tanf(float x) {
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

    /* For very small values, the rounding errors inherant to float values make tan(x)=x a
     * properly rounded approximation. */
    if (xi_abs < 0x39b89ba3) {
        return x;
    }

    /* The core approx function works on the range [-pi/4;pi/4], if we're inside it, no need
     * to reduce the argument. */
    if (xi_abs < 0x3f490fdb) {
        return __tanf(x);
    }

    /* Otherwise, reduce and use the quadrant to choose the right operation to execute. */
    int q;
    double res = __rem_pio2f(x, &q);
    switch (q & 1) {
        case 0:
            return __tanf(res);
        case 1:
            return -__cotf(res);
        default:
            unreachable();
    }
}
