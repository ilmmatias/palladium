/* SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

/* IWYU pragma: private, include <kernel/vidp.h> */

#ifndef _KERNEL_DETAIL_VIDPDEFS_H_
#define _KERNEL_DETAIL_VIDPDEFS_H_

#include <kernel/detail/viddefs.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, vidpdefs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, vidpdefs.h)
#endif /* __has__include */
/* clang-format on */

#endif /* _KERNEL_DETAIL_VIDPDEFS_H_ */
