/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_PSDEFS_H_
#define _KERNEL_DETAIL_PSDEFS_H_

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, psdefs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, psdefs.h)
#endif /* __has__include */
/* clang-format on */

#define PS_STATE_IDLE 1
#define PS_STATE_CREATED 2
#define PS_STATE_QUEUED 3
#define PS_STATE_RUNNING 4
#define PS_STATE_WAITING 5
#define PS_STATE_SUSPENDED 6
#define PS_STATE_TERMINATED 7

#define PS_CREATE_THREAD_DEFAULT 0x0000000000000000
#define PS_CREATE_THREAD_SUSPENDED 0x0000000000000001

#define PS_INIT_ALERT_NONE 0x0000000000000000
#define PS_INIT_ALERT_POOL_ALLOCATED 0x0000000000000001
#define PS_INIT_ALERT_DEFAULT PS_ALERT_POOL_ALLOCATED

#endif /* _KERNEL_DETAIL_PSDEFS_H_ */
