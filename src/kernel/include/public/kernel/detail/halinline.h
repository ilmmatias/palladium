/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_HALINLINE_H_
#define _KERNEL_DETAIL_HALINLINE_H_

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, halinline.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, halinline.h)
#endif /* __has__include */
/* clang-format on */

#endif /* _KERNEL_DETAIL_HALINLINE_H_ */
