/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_KDDEFS_H_
#define _KERNEL_DETAIL_KDDEFS_H_

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, kddefs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, kddefs.h)
#endif /* __has__include */
/* clang-format on */

#define KD_ECHO_ENABLED true

#define KD_TYPE_ERROR 0
#define KD_TYPE_TRACE 1
#define KD_TYPE_DEBUG 2
#define KD_TYPE_INFO 3

#endif /* _KERNEL_DETAIL_KDDEFS_H_ */
