/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef STDINT_H
#define STDINT_H

#define __STDC_VERSION_STDINT_H__ 202311L

/* Width of exact-width signed integer types. */
#define INT8_WIDTH 8
#define INT16_WIDTH 16
#define INT32_WIDTH 32
#define INT64_WIDTH 64

/* Width of exact-width unsigned integer types. */
#define UINT8_WIDTH 8
#define UINT16_WIDTH 16
#define UINT32_WIDTH 32
#define UINT64_WIDTH 64

/* Width of minimum-width signed integer types. */
#define INT_LEAST8_WIDTH __INT_LEAST8_WIDTH__
#define INT_LEAST16_WIDTH __INT_LEAST16_WIDTH__
#define INT_LEAST32_WIDTH __INT_LEAST32_WIDTH__
#define INT_LEAST64_WIDTH __INT_LEAST64_WIDTH__

/* Width of minimum-width unsigned integer types. */
#define UINT_LEAST8_WIDTH __INT_LEAST8_WIDTH__
#define UINT_LEAST16_WIDTH __INT_LEAST16_WIDTH__
#define UINT_LEAST32_WIDTH __INT_LEAST32_WIDTH__
#define UINT_LEAST64_WIDTH __INT_LEAST64_WIDTH__

/* Width of fastest minimum-width signed integer types. */
#define INT_FAST8_WIDTH __INT_FAST8_WIDTH__
#define INT_FAST16_WIDTH __INT_FAST16_WIDTH__
#define INT_FAST32_WIDTH __INT_FAST32_WIDTH__
#define INT_FAST64_WIDTH __INT_FAST64_WIDTH__

/* Width of fastest minimum-width unsigned integer types. */
#define UINT_FAST8_WIDTH __INT_FAST8_WIDTH__
#define UINT_FAST16_WIDTH __INT_FAST16_WIDTH__
#define UINT_FAST32_WIDTH __INT_FAST32_WIDTH__
#define UINT_FAST64_WIDTH __INT_FAST64_WIDTH__

/* Width of integer types capable of holding object pointers. */
#define INTPTR_WIDTH __INTPTR_WIDTH__
#define UINTPTR_WIDTH __UINTPTR_WIDTH__

/* Width of greatest-width integer types. */
#define INTMAX_WIDTH __INTMAX_WIDTH__
#define UINTMAX_WIDTH __UINTMAX_WIDTH__

/* Width of other integer types. */
#define PTRDIFF_WIDTH __PTRDIFF_WIDTH__
#define SIG_ATOMIC_WIDTH __SIG_ATOMIC_WIDTH__
#define SIZE_WIDTH __SIZE_WIDTH__
#define WCHAR_WIDTH __WCHAR_WIDTH__
#define WINT_WIDTH __WINT_WIDTH____

/* Minimal values of exact-width signed integer types. */
#define INT8_MIN (-__INT8_MAX__ - 1)
#define INT16_MIN (-__INT16_MAX__ - 1)
#define INT32_MIN (-__INT32_MAX__ - 1)
#define INT64_MIN (-__INT64_MAX__ - 1)

/* Minimal values of minimum-width signed integer types. */
#define INT_LEAST8_MIN (-__INT_LEAST8_MAX__ - 1)
#define INT_LEAST16_MIN (-__INT_LEAST16_MAX__ - 1)
#define INT_LEAST32_MIN (-__INT_LEAST32_MAX__ - 1)
#define INT_LEAST64_MIN (-__INT_LEAST64_MAX__ - 1)

/* Minimal values of fastest minimum-width signed integer types. */
#define INT_FAST8_MIN (-__INT_FAST8_MAX__ - 1)
#define INT_FAST16_MIN (-__INT_FAST16_MAX__ - 1)
#define INT_FAST32_MIN (-__INT_FAST32_MAX__ - 1)
#define INT_FAST64_MIN (-__INT_FAST64_MAX__ - 1)

/* Minimal values of integer types capable of holding object pointers. */
#define INTPTR_MIN (-__INTPTR_MAX__ - 1)

/* Minimal values of greatest-width integer types. */
#define INTMAX_MIN (-__INTMAX_MAX__ - 1)

/* Minimal values of other integer types. */
#define PTRDIFF_MIN (-__PTRDIFF_MAX__ - 1)
#define SIG_ATOMIC_MIN (-__SIG_ATOMIC_MAX__ - 1)

#ifdef __WCHAR_UNSIGNED__
#define WCHAR_MIN 0
#elif defined(__WCHAR_MIN__)
#define WCHAR_MIN __WCHAR_MIN__
#else
#define WCHAR_MIN (-__WCHAR_MAX__ - 1)
#endif

#ifdef __WINT_UNSIGNED__
#define WINT_MIN 0
#elif defined(__WINT_MIN__)
#define WINT_MIN __WINT_MIN__
#else
#define WINT_MIN (-__WINT_MAX__ - 1)
#endif

/* Maximal values of exact-width signed integer types. */
#define INT8_MAX __INT8_MAX__
#define INT16_MAX __INT16_MAX__
#define INT32_MAX __INT32_MAX__
#define INT64_MAX __INT64_MAX__

/* Maximal values of exact-width unsigned integer types. */
#define UINT8_MAX __UINT8_MAX__
#define UINT16_MAX __UINT16_MAX__
#define UINT32_MAX __UINT32_MAX__
#define UINT64_MAX __UINT64_MAX__

/* Maximal values of minimum-width signed integer types. */
#define INT_LEAST8_MAX __INT_LEAST8_MAX__
#define INT_LEAST16_MAX __INT_LEAST16_MAX__
#define INT_LEAST32_MAX __INT_LEAST32_MAX__
#define INT_LEAST64_MAX __INT_LEAST64_MAX__

/* Maximal values of minimum-width unsigned integer types. */
#define UINT_LEAST8_MAX __UINT_LEAST8_MAX__
#define UINT_LEAST16_MAX __UINT_LEAST16_MAX__
#define UINT_LEAST32_MAX __UINT_LEAST32_MAX__
#define UINT_LEAST64_MAX __UINT_LEAST64_MAX__

/* Maximal values of fastest minimum-width signed integer types. */
#define INT_LEAST8_MAX __INT_LEAST8_MAX__
#define INT_LEAST16_MAX __INT_LEAST16_MAX__
#define INT_LEAST32_MAX __INT_LEAST32_MAX__
#define INT_LEAST64_MAX __INT_LEAST64_MAX__

/* Maximal values of fastest minimum-width unsigned integer types. */
#define UINT_FAST8_MAX __UINT_FAST8_MAX__
#define UINT_FAST16_MAX __UINT_FAST16_MAX__
#define UINT_FAST32_MAX __UINT_FAST32_MAX__
#define UINT_FAST64_MAX __UINT_FAST64_MAX__

/* Maximal values of integer types capable of holding object pointers. */
#define INTPTR_MAX __INTPTR_MAX__
#define UINTPTR_MAX __UINTPTR_MAX__

/* Maximal values of greatest-width integer types. */
#define INTMAX_MAX __INTMAX_MAX__
#define UINTMAX_MAX __UINTMAX_MAX__

/* Maximal values of other integer types. */
#define PTRDIFF_MAX __PTRDIFF_MAX__
#define SIG_ATOMIC_MAX __SIG_ATOMIC_MAX__
#define WCHAR_MAX __WCHAR_MAX__
#define WINT_MAX __WINT_MAX__

/* Macros for minimum-width signed integer constants. */
#define INT8_C(value) (value)
#define INT16_C(value) (value)
#define INT32_C(value) (value)
#define INT64_C(value) (value##LL)

/* Macros for minimum-width unsigned integer constants. */
#define UINT8_C(value) (value)
#define UINT16_C(value) (value)
#define UINT32_C(value) (value##U)
#define UINT64_C(value) (value##ULL)

/* Macros for greatest-width integer constants. */
#if UINTPTR_MAX == UINT64_MAX
#define INTMAX_C(value) (value##LL)
#define UINTMAX_C(value) (value##ULL)
#else
#define INTMAX_C(value) (value##L)
#define UINTMAX_C(value) (value##UL)
#endif /* UINTPTR_MAX == UINT64_MAX */

/* Exact-width signed integer types. */
typedef __INT8_TYPE__ int8_t;
typedef __INT16_TYPE__ int16_t;
typedef __INT32_TYPE__ int32_t;
typedef __INT64_TYPE__ int64_t;

/* Exact-width unsigned integer types. */
typedef __UINT8_TYPE__ uint8_t;
typedef __UINT16_TYPE__ uint16_t;
typedef __UINT32_TYPE__ uint32_t;
typedef __UINT64_TYPE__ uint64_t;

/* Minimum-width signed integer types. */
typedef __INT_LEAST8_TYPE__ int_least8_t;
typedef __INT_LEAST16_TYPE__ int_least16_t;
typedef __INT_LEAST32_TYPE__ int_least32_t;
typedef __INT_LEAST64_TYPE__ int_least64_t;

/* Minimum-width unsigned integer types. */
typedef __UINT_LEAST8_TYPE__ uint_least8_t;
typedef __UINT_LEAST16_TYPE__ uint_least16_t;
typedef __UINT_LEAST32_TYPE__ uint_least32_t;
typedef __UINT_LEAST64_TYPE__ uint_least64_t;

/* Fastest minimum-width signed integer types. */
typedef __INT_FAST8_TYPE__ int_fast8_t;
typedef __INT_FAST16_TYPE__ int_fast16_t;
typedef __INT_FAST32_TYPE__ int_fast32_t;
typedef __INT_FAST64_TYPE__ int_fast64_t;

/* Fastest minimum-width unsigned integer types. */
typedef __UINT_FAST8_TYPE__ uint_fast8_t;
typedef __UINT_FAST16_TYPE__ uint_fast16_t;
typedef __UINT_FAST32_TYPE__ uint_fast32_t;
typedef __UINT_FAST64_TYPE__ uint_fast64_t;

/* Integer types capable of holding object pointers. */
typedef __INTPTR_TYPE__ intptr_t;
typedef __UINTPTR_TYPE__ uintptr_t;

/* Greatest-width integer types. */
typedef __INTMAX_TYPE__ intmax_t;
typedef __UINTMAX_TYPE__ uintmax_t;

#endif /* STDINT_H */
