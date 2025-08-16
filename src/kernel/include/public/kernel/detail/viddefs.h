/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_VIDDEFS_H_
#define _KERNEL_DETAIL_VIDDEFS_H_

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, viddefs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, viddefs.h)
#endif /* __has__include */
/* clang-format on */

#define VID_COLOR_DEFAULT 0x000000, 0xAAAAAA
#define VID_COLOR_INVERSE 0xAAAAAA, 0x000000
#define VID_COLOR_HIGHLIGHT 0x000000, 0xFFFFFF
#define VID_COLOR_PANIC 0xAA0000, 0xFFFFFF

#endif /* _KERNEL_DETAIL_VIDDEFS_H_ */
