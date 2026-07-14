/* SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

/* IWYU pragma: private, include <kernel/kd.h> */

#ifndef _KERNEL_DETAIL_KDDEFS_H_
#define _KERNEL_DETAIL_KDDEFS_H_

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, kddefs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, kddefs.h)
#endif /* __has__include */
/* clang-format on */

#define KD_TYPE_NONE 0
#define KD_TYPE_ERROR 1
#define KD_TYPE_TRACE 2
#define KD_TYPE_DEBUG 3
#define KD_TYPE_INFO 4

#define KD_ENABLE_TRACE false

#ifdef NDEBUG
#define KD_ENABLE_DEBUG false
#else
#define KD_ENABLE_DEBUG true
#endif /* NDEBUG */

#endif /* _KERNEL_DETAIL_KDDEFS_H_ */
