/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_PSDEFS_H_
#define _KERNEL_DETAIL_PSDEFS_H_

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, psdefs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, psdefs.h)
#endif /* __has__include */
/* clang-format on */

#define PS_YIELD_TYPE_QUEUE 0x00
#define PS_YIELD_TYPE_UNQUEUE 0x01

#endif /* _KERNEL_DETAIL_PSDEFS_H_ */
