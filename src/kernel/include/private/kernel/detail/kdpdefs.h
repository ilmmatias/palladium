/* SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

/* IWYU pragma: private, include <kernel/kdp.h> */

#ifndef _KERNEL_DETAIL_KDPDEFS_H_
#define _KERNEL_DETAIL_KDPDEFS_H_

#include <kernel/detail/kddefs.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, kdpdefs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, kdpdefs.h)
#endif /* __has__include */
/* clang-format on */

/* Valid states for the debugger receive functions; Really only two matter (and exist), early
 * initialization (we need to receive ARP+any incoming debug connect packets), and execution break
 * (we need to receive any incoming non-connect debug packets from the saved IP+port). */

#define KDP_STATE_EARLY 0
#define KDP_STATE_LATE 1

/* Defines related to our custom debugger protocol. */

#define KDP_DEBUG_PACKET_CONNECT_REQ 0x00
#define KDP_DEBUG_PACKET_PRINT 0x01
#define KDP_DEBUG_PACKET_BREAK 0x02
#define KDP_DEBUG_PACKET_READ_PHYSICAL_REQ 0x03
#define KDP_DEBUG_PACKET_READ_VIRTUAL_REQ 0x04
#define KDP_DEBUG_PACKET_READ_PORT_REQ 0x05

#define KDP_DEBUG_PACKET_CONNECT_ACK 0x80
#define KDP_DEBUG_PACKET_READ_PHYSICAL_ACK 0x83
#define KDP_DEBUG_PACKET_READ_VIRTUAL_ACK 0x84
#define KDP_DEBUG_PACKET_READ_PORT_ACK 0x85

/* Should this be in here, or somewhere else? */

#define KDP_ANSI_FG_RED "\033[38;5;196m"
#define KDP_ANSI_FG_GREEN "\033[38;5;46m"
#define KDP_ANSI_FG_YELLOW "\033[38;5;226m"
#define KDP_ANSI_FG_BLUE "\033[38;5;51m"

#define KDP_ANSI_RESET "\033[0m"

#endif /* _KERNEL_DETAIL_KDPDEFS_H_ */
