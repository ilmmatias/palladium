/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_KDPFUNCS_H_
#define _KERNEL_DETAIL_KDPFUNCS_H_

#include <kernel/detail/kdfuncs.h>
#include <kernel/detail/kdptypes.h>
#include <kernel/detail/kitypes.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, kdpfuncs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, kdpfuncs.h)
#endif /* __has__include */
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void KdpInitializeDebugger(KiLoaderBlock *LoaderBlock);
void KdpInitializeDeviceDescriptor(KiLoaderBlock *LoaderBlock);
void KdpInitializeExports(void);
void KdpInitializeImports(void);

void KdpParseEthernetFrame(KdpEthernetHeader *EthFrame, uint32_t Length);

uint32_t KdpSendArpPacket(
    uint16_t Type,
    uint8_t DestinationEthernetAddress[6],
    uint8_t DestinationHardwareAddress[6],
    uint8_t DestinationProtocolAddress[4]);
void KdpParseArpFrame(KdpArpHeader *ArpFrame, uint32_t Length);

uint16_t KdpCalculateIpChecksum(KdpIpHeader *Header);
void KdpParseIpFrame(uint8_t SourceHardwareAddress[6], KdpIpHeader *IpFrame, uint32_t Length);

uint32_t KdpSendUdpPacket(
    uint8_t DestinationHardwareAddress[6],
    uint8_t DestinationProtocolAddress[4],
    uint16_t SourcePort,
    uint16_t DestinationPort,
    void *Buffer,
    size_t Size);
void KdpParseUdpFrame(
    uint8_t SourceHardwareAddress[6],
    uint8_t SourceProtocolAddress[4],
    KdpUdpHeader *UdpFrame,
    uint32_t Length);

void KdpParseDebugPacket(
    uint8_t SourceHardwareAddress[6],
    uint8_t SourceProtocolAddress[4],
    uint16_t SourcePort,
    KdpDebugPacket *Packet,
    uint32_t Length);

void KdpPrintVariadic(const char *Message, va_list Arguments);
__attribute__((format(printf, 1, 2))) void KdpPrint(const char *Message, ...);

uint32_t KdpInitializeController(KdpExtensibilityData *KdNet);
void KdpShutdownController(void *Adapter);
void KdpSetHibernateRange(void);
uint32_t KdpGetRxPacket(void *Adapter, uint32_t *Handle, void **Packet, uint32_t *Length);
void KdpReleaseRxPacket(void *Adapter, uint32_t Handle);
uint32_t KdpGetTxPacket(void *Adapter, uint32_t *Handle);
uint32_t KdpSendTxPacket(void *Adapter, uint32_t Handle, uint32_t Length);
void *KdpGetPacketAddress(void *Adapter, uint32_t Handle);
uint32_t KdpGetPacketLength(void *Adapter, uint32_t Handle);
uint32_t KdpGetHardwareContextSize(KdpDebugDeviceDescriptor *Device);
uint32_t KdpDeviceControl(
    void *Adapter,
    uint32_t RequestCode,
    void *InputBuffer,
    uint32_t InputBufferLength,
    void *OutputBuffer,
    uint32_t OutputBufferLength);
uint32_t KdpReadSerialByte(void *Adapter, uint8_t *Byte);
uint32_t KdpWriteSerialByte(void *Adapter, uint8_t Byte);
uint32_t KdpSerialOutputInit(KdpDebugDeviceDescriptor *Device, KdpPhysicalAddress *Address);
void KdpSerialOutputByte(uint8_t Byte);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _KERNEL_DETAIL_KDPFUNCS_H_ */
