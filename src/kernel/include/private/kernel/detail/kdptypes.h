/* SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

/* IWYU pragma: private, include <kernel/kdp.h> */

#ifndef _KERNEL_DETAIL_KDPTYPES_H_
#define _KERNEL_DETAIL_KDPTYPES_H_

#include <kernel/detail/kdtypes.h>
#include <stdint.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, kdptypes.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, kdptypes.h)
#endif /* __has__include */
/* clang-format on */

/* Some ethernet-related types. */

typedef struct __attribute__((packed)) {
    uint8_t DestinationAddress[6];
    uint8_t SourceAddress[6];
    uint16_t Type;
} KdpEthernetHeader;

typedef struct __attribute__((packed)) {
    uint16_t HardwareType;
    uint16_t ProtocolType;
    uint8_t HardwareAddressLength;
    uint8_t ProtocolAddressLength;
    uint16_t Operation;
    uint8_t SourceHardwareAddress[6];
    uint8_t SourceProtocolAddress[4];
    uint8_t DestinationHardwareAddress[6];
    uint8_t DestinationProtocolAddress[4];
} KdpArpHeader;

typedef struct __attribute__((packed)) {
    uint8_t Length : 4;
    uint8_t Version : 4;
    uint8_t TypeOfService;
    uint16_t TotalLength;
    uint16_t Identifier;
    uint16_t FragmentOffset : 13;
    uint16_t Flags : 3;
    uint8_t TimeToLive;
    uint8_t Protocol;
    uint16_t HeaderChecksum;
    uint8_t SourceAddress[4];
    uint8_t DestinationAddress[4];
} KdpIpHeader;

typedef struct __attribute__((packed)) {
    uint16_t SourcePort;
    uint16_t DestinationPort;
    uint16_t Length;
    uint16_t Checksum;
} KdpUdpHeader;

/* And finally our custom debugger protocol types (which slot right on top of UDP). */

typedef struct __attribute__((packed)) {
    uint8_t Type;
} KdpDebugPacket;

typedef struct __attribute__((packed)) {
    uint8_t Type;
    char Architecture[16];
} KdpDebugConnectAckPacket;

typedef struct __attribute__((packed)) {
    uint8_t Type;
    uint64_t Address;
    uint8_t ItemSize;
    uint32_t ItemCount;
    uint32_t Length;
} KdpDebugReadAddressPacket;

typedef struct __attribute__((packed)) {
    uint8_t Type;
    uint64_t Address;
    uint8_t Size;
} KdpDebugReadPortReqPacket;

typedef struct __attribute__((packed)) {
    uint8_t Type;
    uint64_t Address;
    uint8_t Size;
    uint32_t Value;
} KdpDebugReadPortAckPacket;

#endif /* _KERNEL_DETAIL_KDPTYPES_H_ */
