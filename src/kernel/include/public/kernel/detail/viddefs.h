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

#define VID_MESSAGE_ERROR 0
#define VID_MESSAGE_TRACE 1
#define VID_MESSAGE_DEBUG 2
#define VID_MESSAGE_INFO 3

#define VID_ENABLE_MESSAGE_TRACE 0
#define VID_ENABLE_MESSAGE_DEBUG 0
#define VID_ENABLE_MESSAGE_INFO 1

#endif /* _KERNEL_DETAIL_VIDDEFS_H_ */
