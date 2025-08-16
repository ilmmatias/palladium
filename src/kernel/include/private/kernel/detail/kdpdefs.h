/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_KDPDEFS_H_
#define _KERNEL_DETAIL_KDPDEFS_H_

#include <kernel/detail/kddefs.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, kdpdefs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, kdpdefs.h)
#endif /* __has__include */
/* clang-format on */

#endif /* _KERNEL_DETAIL_KDPDEFS_H_ */
