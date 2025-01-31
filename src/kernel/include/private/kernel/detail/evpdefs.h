/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_EVPDEFS_H_
#define _KERNEL_DETAIL_EVPDEFS_H_

#include <kernel/detail/evdefs.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, evpdefs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, evpdefs.h)
#endif /* __has__include */
/* clang-format on */

#define EVP_TICK_PERIOD (1 * EV_MILLISECS)

#endif /* _KERNEL_DETAIL_EVPDEFS_H_ */
