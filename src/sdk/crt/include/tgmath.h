/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef TGMATH_H
#define TGMATH_H

#include <complex.h>
#include <math.h>

#define __STDC_VERSION_TGMATH_H__ 202311L

/* _Generic isn't available in C++ (we need to use overloadable functions instead), so, TODO! */

#define __tgmath_real_1_1(name, x) \
    (_Generic(x, default: name, float: name##f, long double: name##l)(x))

#define __tgmath_real_2_1(name, x, y) \
    (_Generic(x, default: name, float: name##f, long double: name##l)(x, y))

#define __tgmath_real_2_2(name, x, y) \
    (_Generic(x + y, default: name, float: name##f, long double: name##l)(x, y))

#define __tgmath_real_3_1(name, x, y, z) \
    (_Generic(x, default: name, float: name##f, long double: name##l)(x, y, z))

#define __tgmath_real_3_2(name, x, y, z) \
    (_Generic(x + y, default: name, float: name##f, long double: name##l)(x, y, z))

#define __tgmath_real_3_3(name, x, y, z) \
    (_Generic(x + y + z, default: name, float: name##f, long double: name##l)(x, y, z))

#define __tgmath_narrow_1_1(name, x) (_Generic(x, default: name, long double: name##l)(x))

#define __tgmath_narrow_2_2(name, x, y) (_Generic(x + y, default: name, long double: name##l)(x, y))

#define __tgmath_narrow_3_3(name, x, y, z) \
    (_Generic(x + y + z, default: name, long double: name##l)(x, y, z))

#define __tgmath_complex_1_1(name, x) \
    (_Generic(x, default: name, float complex: name##f, long double complex: name##l)(x))

#define __tgmath_any_1_1(name, cname, x) \
    _Generic(                            \
        x,                               \
        default: name,                   \
        float: name##f,                  \
        long double: name##l,            \
        double complex: cname,           \
        float complex: cname##f,         \
        long double complex: cname##l)(x)

#define __tgmath_any_2_2(name, cname, x, y) \
    _Generic(                               \
        x,                                  \
        y,                                  \
        default: name,                      \
        float: name##f,                     \
        long double: name##l,               \
        double complex: cname,              \
        float complex: cname##f,            \
        long double complex: cname##l)(x, y)

/* Trigonometric functions. */
#define acos(x) __tgmath_any_1_1(acos, cacos, (x))
#define asin(x) __tgmath_any_1_1(asin, casin, (x))
#define atan(x) __tgmath_any_1_1(atan, catan, (x))
#define atan2(x, y) __tgmath_real_2_2(atan2, (x), (y))
#define cos(x) __tgmath_any_1_1(cos, ccos, (x))
#define sin(x) __tgmath_any_1_1(sin, csin, (x))
#define tan(x) __tgmath_any_1_1(tan, ctan, (x))
#define acospi(x) __tgmath_real_1_1(acospi, (x))
#define asinpi(x) __tgmath_real_1_1(asinpi, (x))
#define atanpi(x) __tgmath_real_1_1(atanpi, (x))
#define atan2pi(x, y) __tgmath_real_2_2(atan2pi, (x), (y))
#define cospi(x) __tgmath_real_1_1(cospi, (x))
#define sinpi(x) __tgmath_real_1_1(sinpi, (x))
#define tanpi(x) __tgmath_real_1_1(tanpi, (x))

/* Hyperbolic functions. */
#define acosh(x) __tgmath_any_1_1(acosh, cacosh, (x))
#define asinh(x) __tgmath_any_1_1(asinh, casinh, (x))
#define atanh(x) __tgmath_any_1_1(atanh, catanh, (x))
#define cosh(x) __tgmath_any_1_1(cosh, ccosh, (x))
#define sinh(x) __tgmath_any_1_1(sinh, csinh, (x))
#define tanh(x) __tgmath_any_1_1(tanh, ctanh, (x))

/* Exponential and logarithmic functions. */
#define exp(x) __tgmath_any_1_1(exp, cexp, (x))
#define exp10(x) __tgmath_real_1_1(exp10, (x))
#define exp10m1(x) __tgmath_real_1_1(exp10m1, (x))
#define exp2(x) __tgmath_real_1_1(exp2, (x))
#define exp2m1(x) __tgmath_real_1_1(exp2m1, (x))
#define expm1(x) __tgmath_real_1_1(expm1, (x))
#define frexp(x, y) __tgmath_real_2_1(frexp, (x), (y))
#define ilogb(x) __tgmath_real_1_1(ilogb, (x))
#define ldexp(x, y) __tgmath_real_2_1(ldexp, (x), (y))
#define llogb(x) __tgmath_real_1_1(llogb, (x))
#define log(x) __tgmath_any_1_1(log, clog, (x))
#define log10(x) __tgmath_real_1_1(log10, (x))
#define log10p1(x) __tgmath_real_1_1(log10p1, (x))
#define log1p(x) __tgmath_real_1_1(log1p, (x))
#define logp1(x) __tgmath_real_1_1(logp1, (x))
#define log2(x) __tgmath_real_1_1(log2, (x))
#define log2p1(x) __tgmath_real_1_1(log2p1, (x))
#define logb(x) __tgmath_real_1_1(logb, (x))
#define scalbn(x, y) __tgmath_real_2_1(scalbn, (x), (y))
#define scalbln(x, y) __tgmath_real_2_1(scalbln, (x), (y))

/* Power and absolute-value functions. */
#define cbrt(x) __tgmath_real_1_1(cbrt, (x))
#define compoundn(x, y) __tgmath_real_2_1(compoundn, (x), (y))
#define fabs(x) __tgmath_any_1_1(fabs, cabs, (x))
#define hypot(x, y) __tgmath_real_2_2(hypot, (x), (y))
#define pow(x, y) __tgmath_any_2_2(pow, cpow, (x), (y))
#define pown(x, y) __tgmath_real_2_1(pown, (x), (y))
#define powr(x, y) __tgmath_real_2_2(powr, (x), (y))
#define rootn(x, y) __tgmath_real_2_1(rootn, (x), (y))
#define rsqrt(x) __tgmath_real_1_1(rsqrt, (x))
#define sqrt(x) __tgmath_any_1_1(sqrt, csqrt, (x))

/* Error and gamma functions. */
#define erf(x) __tgmath_real_1_1(erf, (x))
#define erfc(x) __tgmath_real_1_1(erfc, (x))
#define lgamma(x) __tgmath_real_1_1(lgamma, (x))
#define tgamma(x) __tgmath_real_1_1(tgamma, (x))

/* Nearest integer functions. */
#define ceil(x) __tgmath_real_1_1(ceil, (x))
#define floor(x) __tgmath_real_1_1(floor, (x))
#define nearbyint(x) __tgmath_real_1_1(nearbyint, (x))
#define rint(x) __tgmath_real_1_1(rint, (x))
#define lrint(x) __tgmath_real_1_1(lrint, (x))
#define llrint(x) __tgmath_real_1_1(llrint, (x))
#define round(x) __tgmath_real_1_1(round, (x))
#define lround(x) __tgmath_real_1_1(lround, (x))
#define llround(x) __tgmath_real_1_1(llround, (x))
#define roundeven(x) __tgmath_real_1_1(roundeven, (x))
#define trunc(x) __tgmath_real_1_1(trunc, (x))
#define fromfp(x, y, z) __tgmath_real_3_1(fromfp, (x), (y), (z))
#define ufromfp(x, y, z) __tgmath_real_3_1(ufromfp, (x), (y), (z))
#define fromfpx(x, y, z) __tgmath_real_3_1(fromfpx, (x), (y), (z))
#define ufromfpx(x, y, z) __tgmath_real_3_1(ufromfpx, (x), (y), (z))

/* Remainder functions. */
#define fmod(x, y) __tgmath_real_2_2(fmod, (x), (y))
#define remainder(x, y) __tgmath_real_2_2(remainder, (x), (y))
#define remquo(x, y, z) __tgmath_real_3_2(remquo, (x), (y), (z))

/* Manipulation functions. */
#define copysign(x, y) __tgmath_real_2_2(copysign, (x), (y))
#define nextafter(x, y) __tgmath_real_2_2(nextafter, (x), (y))
#define nexttoward(x, y) __tgmath_real_2_2(nexttoward, (x), (y))
#define nextup(x) __tgmath_real_1_1(nextup, (x))
#define nextdown(x) __tgmath_real_1_1(nextdown, (x))
#define carg(x) __tgmath_complex_1_1(carg, (x))
#define cimag(x) __tgmath_complex_1_1(cimag, (x))
#define conj(x) __tgmath_complex_1_1(conj, (x))
#define cproj(x) __tgmath_complex_1_1(cproj, (x))
#define creal(x) __tgmath_complex_1_1(creal, (x))

/* Maximum, minimum, and positive difference functions. */
#define fdim(x, y) __tgmath_real_2_2(fdim, (x), (y))
#define fmax(x, y) __tgmath_real_2_2(fmax, (x), (y))
#define fmin(x, y) __tgmath_real_2_2(fmin, (x), (y))
#define fmaximum(x, y) __tgmath_real_2_2(fmaximum, (x), (y))
#define fmaximum_mag(x, y) __tgmath_real_2_2(fmaximum_mag, (x), (y))
#define fminimum_mag(x, y) __tgmath_real_2_2(fminimum_mag, (x), (y))
#define fmaximum_num(x, y) __tgmath_real_2_2(fmaximum_num, (x), (y))
#define fminimum_num(x, y) __tgmath_real_2_2(fminimum_num, (x), (y))
#define fmaximum_mag_num(x, y) __tgmath_real_2_2(fmaximum_mag_num, (x), (y))
#define fminimum_mag_num(x, y) __tgmath_real_2_2(fminimum_mag_num, (x), (y))

/* Fused multiply-add. */
#define fma(x, y, z) __tgmath_real_3_3(fma, (x), (y), (z))

/* Functions that round result to narrower type. */
#define fadd(x, y) __tgmath_narrow_2_2(fadd, (x), (y))
#define dadd(x, y) (daddl((x), (y)))
#define fsub(x, y) __tgmath_narrow_2_2(fsub, (x), (y))
#define dsub(x, y) (dsubl((x), (y)))
#define fmul(x, y) __tgmath_narrow_2_2(fmul, (x), (y))
#define dmul(x, y) (dmull((x), (y)))
#define fdiv(x, y) __tgmath_narrow_2_2(fdiv, (x), (y))
#define ddiv(x, y) (ddivl((x), (y)))
#define ffma(x, y, z) __tgmath_narrow_3_3(ffma, (x), (y), (z))
#define dfma(x, y, z) (dfmal((x), (y), (z)))
#define fsqrt(x) __tgmath_narrow_1_1(fsqrt, (x))
#define dsqrt(x) (dsqrtl((x)))

#undef __tgmath_any_2_2
#undef __tgmath_any_1_1
#undef __tgmath_complex_1_1
#undef __tgmath_narrow_3_3
#undef __tgmath_narrow_2_2
#undef __tgmath_narrow_1_1
#undef __tgmath_real_3_3
#undef __tgmath_real_3_2
#undef __tgmath_real_3_1
#undef __tgmath_real_2_2
#undef __tgmath_real_2_1
#undef __tgmath_real_1_1

#endif /* TGMATH_H */
