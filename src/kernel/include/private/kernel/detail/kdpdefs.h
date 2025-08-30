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

/* Sync these with the equivalent defines from the WDK matching the wanted source of the KDNET
 * drivers. */

#define KDP_DEVICE_FLAGS_HAL_SCRATCH_ALLOCATED 0x01
#define KDP_DEVICE_FLAGS_BARS_MAPPED 0x02
#define KDP_DEVICE_FLAGS_SCRATCH_ALLOCATED 0x04
#define KDP_DEVICE_FLAGS_UNCACHED_MEMORY 0x08

#define KDP_RESOURCE_NULL 0
#define KDP_RESOURCE_PORT 1
#define KDP_RESOURCE_INTERRUPT 2
#define KDP_RESOURCE_MEMORY 3
#define KDP_RESOURCE_DMA 4
#define KDP_RESOURCE_DEVICE_SPECIFIC 5
#define KDP_RESOURCE_BUS_NUMBER 6
#define KDP_RESOURCE_MEMORY_LARGE 7

#define KDP_NAMESPACE_PCI 0
#define KDP_NAMESPACE_ACPI 1
#define KDP_NAMESPACE_ANY 2
#define KDP_NAMESPACE_NONE 3
#define KDP_NAMESPACE_MA 4

#define KDP_ENVIRONMENT_UNSPECIFIED 0
#define KDP_ENVIRONMENT_BOOT 1
#define KDP_ENVIRONMENT_KERNEL 2
#define KDP_ENVIRONMENT_HYPERVISOR 3
#define KDP_ENVIRONMENT_SK 4
#define KDP_ENVIRONMENT_HCL 5

#define KDP_EXTENSIBILITY_EXPORT_COUNT 15
#define KDP_EXTENSIBILITY_IMPORT_COUNT 33

#define KDP_TRANSMIT_LAST 0x20000000
#define KDP_TRANSMIT_HANDLE 0x40000000
#define KDP_TRANSMIT_ASYNC 0x80000000
#define KDP_HANDLE_FLAGS (KDP_TRANSMIT_LAST | KDP_TRANSMIT_HANDLE | KDP_TRANSMIT_ASYNC)

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
