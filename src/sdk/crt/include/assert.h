/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef ASSERT_H
#define ASSERT_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef NDEBUG
#define assert(condition) ((void)0)
#else
#define assert(condition) \
    (void)((condition) || (__assert(#condition, __FILE__, __LINE__, __func__), 0))
#endif /* NDEBUG */

void __assert(const char *condition, const char *file, int line, const char *func);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ASSERT_H */
