/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef STDDEF_H
#define STDDEF_H

#define __STDC_VERSION_STDDEF_H__ 202311L

#undef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void*)0)
#endif /* __cplusplus */

#define unreachable() __builtin_unreachable()
#define offsetof(type, member) __builtin_offsetof(type, member)

typedef __PTRDIFF_TYPE__ ptrdiff_t;
typedef __SIZE_TYPE__ size_t;

/* Other implementations (including the GCC and Clang system headers) use something like this, so it
 * should be okay to do it as well? */
typedef union {
    long long __ll;
    long double __ld;
} max_align_t;

typedef __WCHAR_TYPE__ wchar_t;

/* Either define nullptr_t using __typeof__ (for C, should be good enough for both GCC and Clang),
 * or using decltype (for C++). */
#if defined(__cplusplus) && __cplusplus >= 201103L
typedef decltype(nullptr) nullptr_t;
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
typedef __typeof__(nullptr) nullptr_t;
#endif /* __cplusplus || __STDC_VERSION__ */

#endif /* STDDEF_H */
