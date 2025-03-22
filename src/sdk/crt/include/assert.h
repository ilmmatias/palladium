/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef ASSERT_H
#define ASSERT_H

#define __STDC_VERSION_ASSERT_H__ 202311L

/* -std=c23 already has static_assert defined as a keyword, so only redef it on STDC<23. */
#if !defined(__cplusplus) && __STDC__ < 202311L
#define static_assert _Static_assert
#endif

/* Program diagnostics. */
#ifdef NDEBUG
#define assert(condition) ((void)0)
#else
#define assert(condition) \
    (void)((condition) || (__assert(#condition, __FILE__, __LINE__, __func__), 0))
#endif /* NDEBUG */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

[[noreturn]]
void __assert(const char *condition, const char *file, int line, const char *func);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ASSERT_H */
