/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef CRT_IMPL_COMMON_H
#define CRT_IMPL_COMMON_H

/* Both C>=23 and C++>=11, we can use [[noreturn]].
 * C>=11 can use _Noreturn (either directly, or via the noreturn macro on stdnoreturn.h.
 * Anything else needs the compiler specific attribute. */
#if (defined(__cplusplus) && __cplusplus >= 201103L) || \
    (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L)
#define CRT_NORETURN [[noreturn]]
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define CRT_NORETURN _Noreturn
#else
#define CRT_NORETURN __attribute__((noreturn))
#endif /* __cplusplus || __STDC_VERSION__ */

/* C++ needs either the compiler-specific restrict keyword (which is what we'll use here), or just
 * to stub it out. */
#ifdef __cplusplus
#define CRT_RESTRICT __restrict__
#else
#define CRT_RESTRICT restrict
#endif /* __cplusplus */

#define CRT_ALIAS(x) __attribute__((alias(x)))
#define CRT_ALLOC_ALIGN(x) __attribute__((alloc_align(x)))
#define CRT_ALLOC_SIZE(...) __attribute__((alloc_size(__VA_ARGS__)))
#define CRT_CONSTRUCTOR(...) __attribute__((constructor(__VA_ARGS__)))
#define CRT_DESTRUCTOR(...) __attribute__((destructor(__VA_ARGS__)))
#define CRT_DEPRECATED(...) __attribute__((deprecated(__VA_ARGS__)))
#define CRT_FORMAT(x, y, z) __attribute__((format(x, y, z)))
#define CRT_FORMAT_ARG(x) __attribute__((format_arg(x)))
#define CRT_LIKELY(x) (__builtin_expect((x), 1))
#define CRT_MALLOC __attribute__((malloc))
#define CRT_NONNULL(...) __attribute__((nonnull(__VA_ARGS__)))
#define CRT_UNLIKELY(x) (__builtin_expect((x), 0))
#define CRT_UNUSED __attribute__((unused))
#define CRT_USED __attribute__((used))
#define CRT_WEAK __attribute__((weak))

#endif /* CRT_IMPL_COMMON_H */
