/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_OBPDEFS_H_
#define _KERNEL_DETAIL_OBPDEFS_H_

#include <kernel/detail/obdefs.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, obpdefs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, obpdefs.h)
#endif /* __has__include */
/* clang-format on */

#endif /* _KERNEL_DETAIL_OBPDEFS_H_ */
