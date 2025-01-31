/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_VIDPINLINE_H_
#define _KERNEL_DETAIL_VIDPINLINE_H_

#include <kernel/detail/vidinline.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, vidpinline.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, vidpinline.h)
#endif /* __has__include */
/* clang-format on */

#endif /* _KERNEL_DETAIL_VIDPINLINE_H_ */
