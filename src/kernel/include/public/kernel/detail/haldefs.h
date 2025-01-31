/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_HALDEFS_H_
#define _KERNEL_DETAIL_HALDEFS_H_

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, haldefs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, haldefs.h)
#endif /* __has__include */
/* clang-format on */

#define HAL_NO_EVENT 0
#define HAL_PANIC_EVENT 1

#define HAL_TIMER_WIDTH_32B 32
#define HAL_TIMER_WIDTH_64B 64

#define HAL_INT_POLARITY_HIGH 0
#define HAL_INT_POLARITY_LOW 1

#define HAL_INT_TRIGGER_EDGE 0
#define HAL_INT_TRIGGER_LEVEL 1

#endif /* _KERNEL_DETAIL_HALDEFS_H_ */
