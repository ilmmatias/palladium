/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_MMDEFS_H_
#define _KERNEL_DETAIL_MMDEFS_H_

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, mmdefs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, mmdefs.h)
#endif /* __has__include */
/* clang-format on */

#define MM_PAGE_SIZE (1ull << (MM_PAGE_SHIFT))

#endif /* _KERNEL_DETAIL_MMDEFS_H_ */
