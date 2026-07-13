/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

/* IWYU pragma: private, include <kernel/ki.h> */

#ifndef _KERNEL_DETAIL_KIDEFS_H_
#define _KERNEL_DETAIL_KIDEFS_H_

#include <kernel/detail/kedefs.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, kidefs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, kidefs.h)
#endif /* __has__include */
/* clang-format on */

#define KI_LOADER_MAGIC "OLDR"
#define KI_LOADER_VERSION 0x0000'0000'00000008

#define KI_LOADER_DEBUG_TRANSPORT_NONE 0
#define KI_LOADER_DEBUG_TRANSPORT_KDNET 1
#define KI_LOADER_DEBUG_TRANSPORT_PC16550_PIO 2

#define KI_LOADER_DEBUG_FLAG_ECHO_ENABLED (1u << 0)

#define KI_LOADER_DEBUG_DISCONNECT_STOP 0
#define KI_LOADER_DEBUG_DISCONNECT_CONTINUE 1

#endif /* _KERNEL_DETAIL_KIDEFS_H_ */
