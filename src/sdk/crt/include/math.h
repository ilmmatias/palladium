/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef MATH_H
#define MATH_H

#include <errno.h>
#include <fenv.h>
#include <float.h>
#include <stdint.h>

#define __STDC_VERSION_MATH_H__ 202311L

/* INFINITY and NAN should come from float.h for us. */
#define HUGE_VAL __builtin_huge_val()
#define HUGE_VALF __builtin_huge_valf()
#define HUGE_VALL __builtin_huge_vall()

/* Number classification macros. */
#define FP_INFINITE 0
#define FP_NAN 1
#define FP_NORMAL 2
#define FP_SUBNORMAL 3
#define FP_ZERO 4

/* Math rounding direction macros. */
#define FP_INT_UPWARD 0
#define FP_INT_DOWNWARD 1
#define FP_INT_TOWARDZERO 2
#define FP_INT_TONEARESTFROMZERO 3
#define FP_INT_TONEAREST 4

#ifdef __FP_FAST_FMAF
#define FP_FAST_FMAF
#endif /* __FP_FAST_FMAF */

#ifdef __FP_FAST_FMA
#define FP_FAST_FMA
#endif /* __FP_FAST_FMA */

#ifdef __FP_FAST_FMAL
#define FP_FAST_FMAL
#endif /* __FP_FAST_FMAL */

#if defined(__FP_LOGB0_IS_MIN) || defined(__FP_LOGB0_MIN)
#define FP_ILOGB0 (-__INT_MAX__ - 1)
#define FP_LLOGB0 (-__LONG_MAX__ - 1)
#else
#define FP_ILOGB0 __INT_MAX__
#define FP_LLOGB0 __LONG_MAX__
#endif /* defined(__FP_LOGB0_IS_MIN) || defined(__FP_LOGB0_MIN) */

#if defined(__FP_LOGBNAN_IS_MIN) || defined(__FP_LOGBNAN_MIN)
#define FP_ILOGBNAN (-__INT_MAX__ - 1)
#define FP_LLOGBNAN (-__LONG_MAX__ - 1)
#else
#define FP_ILOGBNAN __INT_MAX__
#define FP_LLOGBNAN __LONG_MAX__
#endif /* defined(__FP_LOGBNAN_IS_MIN) || defined(__FP_LOGBNAN_MIN) */

#define MATH_ERRNO 1
#define MATH_ERREXCEPT 2

#ifdef __FAST_MATH__
#define math_errhandling 0
#elif defined(__NO_MATH_ERRNO__)
#define math_errhandling (MATH_ERREXCEPT)
#else
#define math_errhandling (MATH_ERRNO | MATH_ERREXCEPT)
#endif /* __FAST_MATH__ */

/* Classification macros. */
#define fpclassify(x) \
    __builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, (x))
#define iscanonical(x) ((void)(__typeof(x))(x), 1)
#define isfinite(x) __builtin_isfinite((x))
#define isinf(x) __builtin_isinf((x))
#define isnan(x) __builtin_isnan((x))
#define isnormal(x) __builtin_isnormal((x))
#define signbit(x) __builtin_signbit((x))
#define issignaling(x) __builtin_issignaling((x))
#define issubnormal(x) (fpclassify((x)) == FP_SUBNORMAL)
#define iszero(x) (fpclassify((x)) == FP_ZERO)

/* Comparison macros. */
#define isgreater(x, y) __builtin_isgreater((x), (y))
#define isgreaterequal(x, y) __builtin_isgreaterequal((x), (y))
#define isless(x, y) __builtin_isless((x), (y))
#define islessequal(x, y) __builtin_islessequal((x), (y))
#define islessgreater(x, y) __builtin_islessgreater((x), (y))
#define isunordered(x, y) __builtin_isunordered((x), (y))

/* Clang still doesn't define __builtin_iseqsig. */
#if __has_builtin(__builtin_iseqsig)
#define iseqsig(x, y) __builtin_iseqsig((x), (y))
#else
#define iseqsig(x, y)                  \
    (__extension__({                   \
        __typeof__(x) __x = (x);       \
        __typeof__(y) __y = (y);       \
        bool __xy = __x <= __y;        \
        bool __yx = __y <= __x;        \
        if (!__xy && !__yx) {          \
            errno = EDOM;              \
            feraiseexcept(FE_INVALID); \
        }                              \
        __xy &&__yx;                   \
    }))
#endif /* __has_builtin(__builtin_iseqsig) */

#if __FLT_EVAL_METHOD__ == 1
typedef double float_t;
typedef double double_t;
#elif __FLT_EVAL_METHOD__ == 2
typedef long double float_t;
typedef long double double_t;
#else
typedef float float_t;
typedef double double_t;
#endif /* __FLT_EVAL_METHOD__ */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Trigonometric functions. */

double acos(double x);
float acosf(float x);
long double acosl(long double x);

double asin(double x);
float asinf(float x);
long double asinl(long double x);

double atan(double x);
float atanf(float x);
long double atanl(long double x);

double atan2(double y, double x);
float atan2f(float y, float x);
long double atan2l(long double y, long double x);

double cos(double x);
float cosf(float x);
long double cosl(long double x);

double sin(double x);
float sinf(float x);
long double sinl(long double x);

double tan(double x);
float tanf(float x);
long double tanl(long double x);

double acospi(double x);
float acospif(float x);
long double acospil(long double x);

double asinpi(double x);
float asinpif(float x);
long double asinpil(long double x);

double atanpi(double x);
float atanpif(float x);
long double atanpil(long double x);

double atan2pi(double y, double x);
float atan2pif(float y, float x);
long double atan2pil(long double y, long double x);

double cospi(double x);
float cospif(float x);
long double cospil(long double x);

double sinpi(double x);
float sinpif(float x);
long double sinpil(long double x);

double tanpi(double x);
float tanpif(float x);
long double tanpil(long double x);

/* Hyperbolic functions. */

double acosh(double x);
float acoshf(float x);
long double acoshl(long double x);

double asinh(double x);
float asinhf(float x);
long double asinhl(long double x);

double atanh(double x);
float atanhf(float x);
long double atanhl(long double x);

double cosh(double x);
float coshf(float x);
long double coshl(long double x);

double sinh(double x);
float sinhf(float x);
long double sinhl(long double x);

double tanh(double x);
float tanhf(float x);
long double tanhl(long double x);

/* Exponential and logarithmic functions. */

double exp(double x);
float expf(float x);
long double expl(long double x);

double exp10(double x);
float exp10f(float x);
long double exp10l(long double x);

double exp10m1(double x);
float exp10m1f(float x);
long double exp10m1l(long double x);

double exp2(double x);
float exp2f(float x);
long double exp2l(long double x);

double exp2m1(double x);
float exp2m1f(float x);
long double exp2m1l(long double x);

double expm1(double x);
float expm1f(float x);
long double expm1l(long double x);

double frexp(double value, int *p);
float frexpf(float value, int *p);
long double frexpl(long double value, int *p);

int ilogb(double x);
int ilogbf(float x);
int ilogbl(long double x);

double ldexp(double x, int p);
float ldexpf(float x, int p);
long double ldexpl(long double x, int p);

long int llogb(double x);
long int llogbf(float x);
long int llogbl(long double x);

double log(double x);
float logf(float x);
long double logl(long double x);

double log10(double x);
float log10f(float x);
long double log10l(long double x);

double log10p1(double x);
float log10p1f(float x);
long double log10p1l(long double x);

double log1p(double x);
float log1pf(float x);
long double log1pl(long double x);

double logp1(double x);
float logp1f(float x);
long double logp1l(long double x);

double log2(double x);
float log2f(float x);
long double log2l(long double x);

double log2p1(double x);
float log2p1f(float x);
long double log2p1l(long double x);

double logb(double x);
float logbf(float x);
long double logbl(long double x);

double modf(double value, double *iptr);
float modff(float value, float *iptr);
long double modfl(long double value, long double *iptr);

double scalbn(double x, int n);
float scalbnf(float x, int n);
long double scalbnl(long double x, int n);

double scalbln(double x, long int n);
float scalblnf(float x, long int n);
long double scalblnl(long double x, long int n);

/* Power and absolute-value functions. */

double cbrt(double x);
float cbrtf(float x);
long double cbrtl(long double x);

double compoundn(double x, long long int n);
float compoundnf(float x, long long int n);
long double compoundnl(long double x, long long int n);

double fabs(double x);
float fabsf(float x);
long double fabsl(long double x);

double hypot(double x, double y);
float hypotf(float x, float y);
long double hypotl(long double x, long double y);

double pow(double x, double y);
float powf(float x, float y);
long double powl(long double x, long double y);

double pown(double x, long long int n);
float pownf(float x, long long int n);
long double pownl(long double x, long long int n);

double powr(double y, double x);
float powrf(float y, float x);
long double powrl(long double y, long double x);

double rootn(double x, long long int n);
float rootnf(float x, long long int n);
long double rootnl(long double x, long long int n);

double rsqrt(double x);
float rsqrtf(float x);
long double rsqrtl(long double x);

double sqrt(double x);
float sqrtf(float x);
long double sqrtl(long double x);

/* Error and gamma functions. */

double erf(double x);
float erff(float x);
long double erfl(long double x);

double erfc(double x);
float erfcf(float x);
long double erfcl(long double x);

double lgamma(double x);
float lgammaf(float x);
long double lgammal(long double x);

double tgamma(double x);
float tgammaf(float x);
long double tgammal(long double x);

/* Nearest integer functions. */

double ceil(double x);
float ceilf(float x);
long double ceill(long double x);

double floor(double x);
float floorf(float x);
long double floorl(long double x);

double nearbyint(double x);
float nearbyintf(float x);
long double nearbyintl(long double x);

double rint(double x);
float rintf(float x);
long double rintl(long double x);

long int lrint(double x);
long int lrintf(float x);
long int lrintl(long double x);

long long int llrint(double x);
long long int llrintf(float x);
long long int llrintl(long double x);

double round(double x);
float roundf(float x);
long double roundl(long double x);

long int lround(double x);
long int lroundf(float x);
long int lroundl(long double x);

long long int llround(double x);
long long int llroundf(float x);
long long int llroundl(long double x);

double roundeven(double x);
float roundevenf(float x);
long double roundevenl(long double x);

double trunc(double x);
float truncf(float x);
long double truncl(long double x);

double fromfp(double x, int rnd, unsigned int width);
float fromfpf(float x, int rnd, unsigned int width);
long double fromfpl(long double x, int rnd, unsigned int width);

double ufromfp(double x, int rnd, unsigned int width);
float ufromfpf(float x, int rnd, unsigned int width);
long double ufromfpl(long double x, int rnd, unsigned int width);

double fromfpx(double x, int rnd, unsigned int width);
float fromfpxf(float x, int rnd, unsigned int width);
long double fromfpxl(long double x, int rnd, unsigned int width);

double ufromfpx(double x, int rnd, unsigned int width);
float ufromfpxf(float x, int rnd, unsigned int width);
long double ufromfpxl(long double x, int rnd, unsigned int width);

/* Remainder functions. */

double fmod(double x, double y);
float fmodf(float x, float y);
long double fmodl(long double x, long double y);

double remainder(double x, double y);
float remainderf(float x, float y);
long double remainderl(long double x, long double y);

double remquo(double x, double y, int *quo);
float remquof(float x, float y, int *quo);
long double remquol(long double x, long double y, int *quo);

/* Manipulation functions. */

double copysign(double x, double y);
float copysignf(float x, float y);
long double copysignl(long double x, long double y);

double nan(const char *tagp);
float nanf(const char *tagp);
long double nanl(const char *tagp);

double nextafter(double x, double y);
float nextafterf(float x, float y);
long double nextafterl(long double x, long double y);

double nexttoward(double x, long double y);
float nexttowardf(float x, long double y);
long double nexttowardl(long double x, long double y);

double nextup(double x);
float nextupf(float x);
long double nextupl(long double x);

double nextdown(double x);
float nextdownf(float x);
long double nextdownl(long double x);

int canonicalize(double *cx, const double *x);
int canonicalizef(float *cx, const float *x);
int canonicalizel(long double *cx, const long double *x);

/* Maximum, minimum, and positive difference functions. */

double fdim(double x, double y);
float fdimf(float x, float y);
long double fdiml(long double x, long double y);

double fmax(double x, double y);
float fmaxf(float x, float y);
long double fmaxl(long double x, long double y);

double fmin(double x, double y);
float fminf(float x, float y);
long double fminl(long double x, long double y);

double fmaximum(double x, double y);
float fmaximumf(float x, float y);
long double fmaximuml(long double x, long double y);

double fmaximum_mag(double x, double y);
float fmaximum_magf(float x, float y);
long double fmaximum_magl(long double x, long double y);

double fminimum_mag(double x, double y);
float fminimum_magf(float x, float y);
long double fminimum_magl(long double x, long double y);

double fmaximum_num(double x, double y);
float fmaximum_numf(float x, float y);
long double fmaximum_numl(long double x, long double y);

double fminimum_num(double x, double y);
float fminimum_numf(float x, float y);
long double fminimum_numl(long double x, long double y);

double fmaximum_mag_num(double x, double y);
float fmaximum_mag_numf(float x, float y);
long double fmaximum_mag_numl(long double x, long double y);

double fminimum_mag_num(double x, double y);
float fminimum_mag_numf(float x, float y);
long double fminimum_mag_numl(long double x, long double y);

/* Fused multiply-add. */

double fma(double x, double y, double z);
float fmaf(float x, float y, float z);
long double fmal(long double x, long double y, long double z);

/* Functions that round result to narrower type. */

float fadd(double x, double y);
float faddl(long double x, long double y);
double daddl(long double x, long double y);

float fsub(double x, double y);
float fsubl(long double x, long double y);
double dsubl(long double x, long double y);

float fmul(double x, double y);
float fmull(long double x, long double y);
double dmull(long double x, long double y);

float fdiv(double x, double y);
float fdivl(long double x, long double y);
double ddivl(long double x, long double y);

float ffma(double x, double y, double z);
float ffmal(long double x, long double y, long double z);
double dfmal(long double x, long double y, long double z);

float fsqrt(double x);
float fsqrtl(long double x);
double dsqrtl(long double x);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MATH_H */
