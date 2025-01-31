/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_PSPDEFS_H_
#define _KERNEL_DETAIL_PSPDEFS_H_

#include <kernel/detail/evpdefs.h>
#include <kernel/detail/psdefs.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, pspdefs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, pspdefs.h)
#endif /* __has__include */
/* clang-format on */

#define PSP_DEFAULT_TICKS ((10 * EV_MILLISECS) / EVP_TICK_PERIOD)

#endif /* _KERNEL_DETAIL_PSPDEFS_H_ */
