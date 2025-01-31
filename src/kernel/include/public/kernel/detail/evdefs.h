/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_EVDEFS_H_
#define _KERNEL_DETAIL_EVDEFS_H_

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, evdefs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, evdefs.h)
#endif /* __has__include */
/* clang-format on */

#define EV_MICROSECS 1000ull
#define EV_MILLISECS 1000000ull
#define EV_SECS 1000000000ull

#endif /* _KERNEL_DETAIL_EVDEFS_H_ */
