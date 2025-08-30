/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_KDPTYPES_H_
#define _KERNEL_DETAIL_KDPTYPES_H_

#include <kernel/detail/kdtypes.h>
#include <stdint.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, kdptypes.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, kdptypes.h)
#endif /* __has__include */
/* clang-format on */

/* Sync these with the equivalent types from the WDK matching the wanted source of the KDNET
 * drivers. */

typedef union {
    struct {
        uint32_t LowPart;
        uint32_t HighPart;
    };
    uint64_t QuadPart;
} KdpPhysicalAddress;

typedef struct {
    uint8_t Type;
    bool Valid;
    uint8_t BitWidth;
    uint8_t AccessSize;
    uint8_t *TranslatedAddress;
    uint32_t Length;
} KdpDebugDeviceAddress;

typedef struct {
    KdpPhysicalAddress Start;
    KdpPhysicalAddress MaxEnd;
    void *VirtualAddress;
    uint32_t Length;
    bool Cached;
    bool Aligned;
} KdpDebugMemoryRequirements;

typedef struct {
    uint32_t HwContextSize;
    uint32_t SharedVisibleDataSize;
    bool UseSerialFraming;
    bool ValidUsbCoreId;
    uint8_t UsbCoreId;
} KdpDebugTransportData;

typedef struct {
    void *PciIoProcotolHandle;
    void *Mapping;
} KdpDebugEfiIoMmuData;

typedef struct {
    uint32_t Bus;
    uint32_t Slot;
    uint16_t Segment;
    uint16_t VendorId;
    uint16_t DeviceId;
    uint8_t BaseClass;
    uint8_t SubClass;
    uint8_t ProgIf;
    uint8_t Flags;
    bool Initialized;
    bool Configured;
    KdpDebugDeviceAddress BaseAddress[6];
    KdpDebugMemoryRequirements Memory;
    uint32_t Dbg2TableIndex;
    uint16_t PortType;
    uint16_t PortSubtype;
    void *OemData;
    uint32_t OemDataLength;
    uint32_t NameSpace;
    uint16_t *NameSpacePath;
    uint32_t NameSpacePathLength;
    uint32_t TransportType;
    KdpDebugTransportData TransportData;
    KdpDebugEfiIoMmuData EfiIoMmuData;
} KdpDebugDeviceDescriptor;

typedef struct {
    void *Hardware;
    KdpDebugDeviceDescriptor *Device;
    uint8_t *TargetMacAddress;
    uint32_t LinkSpeed;
    uint32_t LinkDuplex;
    uint8_t *LinkState;
    uint32_t SerialBaudRate;
    uint32_t Flags;
    uint8_t RestartKdnet;
    uint8_t Reserved[3];
} KdpExtensibilityData;

typedef uint32_t (*KdpInitializeControllerFn)(KdpExtensibilityData *KdNet);
typedef void (*KdpShutdownControllerFn)(void *Adapter);
typedef void (*KdpSetHibernateRangeFn)(void);
typedef uint32_t (
    *KdpGetRxPacketFn)(void *Adapter, uint32_t *Handle, void **Packet, uint32_t *Length);
typedef void (*KdpReleaseRxPacketFn)(void *Adapter, uint32_t Handle);
typedef uint32_t (*KdpGetTxPacketFn)(void *Adapter, uint32_t *Handle);
typedef uint32_t (*KdpSendTxPacketFn)(void *Adapter, uint32_t Handle, uint32_t Length);
typedef void *(*KdpGetPacketAddressFn)(void *Adapter, uint32_t Handle);
typedef uint32_t (*KdpGetPacketLengthFn)(void *Adapter, uint32_t Handle);
typedef uint32_t (*KdpGetHardwareContextSizeFn)(KdpDebugDeviceDescriptor *Device);
typedef uint32_t (*KdpDeviceControlFn)(
    void *Adapter,
    uint32_t RequestCode,
    void *InputBuffer,
    uint32_t InputBufferLength,
    void *OutputBuffer,
    uint32_t OutputBufferLength);
typedef uint32_t (*KdpReadSerialByteFn)(void *Adapter, uint8_t *Byte);
typedef uint32_t (*KdpWriteSerialByteFn)(void *Adapter, uint8_t Byte);
typedef uint32_t (
    *KdpSerialOutputInitFn)(KdpDebugDeviceDescriptor *Device, KdpPhysicalAddress *Address);
typedef void (*KdpSerialOutputByteFn)(uint8_t Byte);

typedef struct {
    uint32_t FunctionCount;
    KdpInitializeControllerFn InitializeController;
    KdpShutdownControllerFn ShutdownController;
    KdpSetHibernateRangeFn SetHibernateRange;
    KdpGetRxPacketFn GetRxPacket;
    KdpReleaseRxPacketFn ReleaseRxPacket;
    KdpGetTxPacketFn GetTxPacket;
    KdpSendTxPacketFn SendTxPacket;
    KdpGetPacketAddressFn GetPacketAddress;
    KdpGetPacketLengthFn GetPacketLength;
    KdpGetHardwareContextSizeFn GetHardwareContextSize;
    KdpDeviceControlFn DeviceControl;
    KdpReadSerialByteFn ReadSerialByte;
    KdpWriteSerialByteFn WriteSerialByte;
    KdpSerialOutputInitFn SerialOutputInit;
    KdpSerialOutputByteFn SerialOutputByte;
} KdpExtensibilityExports;

/* I found what seems to be a strange choice when looking at kdnetextensibility.h to build these
 * types: Why is it that Read/WritePortULong64 take uint32_t pointers and return uint32_ts? That
 * doesn't make much sense, but I'll leave it as is. */

typedef uint32_t (*KdpGetPciDataByOffsetFn)(
    uint32_t BusNumber,
    uint32_t SlotNumber,
    void *Buffer,
    uint32_t Offset,
    uint32_t Length);
typedef uint32_t (*KdpSetPciDataByOffsetFn)(
    uint32_t BusNumber,
    uint32_t SlotNumber,
    void *Buffer,
    uint32_t Offset,
    uint32_t Length);
typedef KdpPhysicalAddress (*KdpGetPhysicalAddressFn)(void *Va);
typedef void (*KdpStallExecutionProcessorFn)(uint32_t Microseconds);
typedef uint8_t (*KdpReadRegisterUCharFn)(uint8_t *Register);
typedef uint16_t (*KdpReadRegisterUShortFn)(uint16_t *Register);
typedef uint32_t (*KdpReadRegisterULongFn)(uint32_t *Register);
typedef uint64_t (*KdpReadRegisterULong64Fn)(uint64_t *Register);
typedef void (*KdpWriteRegisterUCharFn)(uint8_t *Register, uint8_t Value);
typedef void (*KdpWriteRegisterUShortFn)(uint16_t *Register, uint16_t Value);
typedef void (*KdpWriteRegisterULongFn)(uint32_t *Register, uint32_t Value);
typedef void (*KdpWriteRegisterULong64Fn)(uint64_t *Register, uint64_t Value);
typedef uint8_t (*KdpReadPortUCharFn)(uint8_t *Port);
typedef uint16_t (*KdpReadPortUShortFn)(uint16_t *Port);
typedef uint32_t (*KdpReadPortULongFn)(uint32_t *Port);
typedef uint32_t (*KdpReadPortULong64Fn)(uint64_t *Port);
typedef void (*KdpWritePortUCharFn)(uint8_t *Port, uint8_t Value);
typedef void (*KdpWritePortUShortFn)(uint16_t *Port, uint16_t Value);
typedef void (*KdpWritePortULongFn)(uint32_t *Port, uint32_t Value);
typedef void (*KdpWritePortULong64Fn)(uint32_t *Port, uint64_t Value);
typedef void (*KdpSetHiberRangeFn)(
    void *MemoryMap,
    uint32_t Flags,
    void *Address,
    uint32_t Length,
    uint32_t Tag);
typedef void (*KdpBugCheckExFn)(
    uint32_t BugCheckCode,
    uint32_t BugCheckParameter1,
    uint32_t BugCheckParameter2,
    uint32_t BugCheckParameter3,
    uint32_t BugCheckParameter4);
typedef void *(*KdpMapPhysicalMemoryFn)(
    KdpPhysicalAddress PhysicalAddress,
    uint32_t NumberPages,
    bool FlushCurrentTlb);
typedef void (
    *KdpUnmapVirtualAddressFn)(void *VirtualAddress, uint32_t NumberPages, bool FlushCurrentTlb);
typedef uint64_t (*KdpReadCycleCounterFn)(uint64_t *Frequency);
typedef void (*KdpPrintfFn)(char *Format, ...);
typedef bool (*KdpVmbusInitializeFn)(
    void *Context,
    void *Parameters,
    bool UnreserveChannels,
    void *ArrivalRoutine,
    void *ArrivalRoutineContext,
    uint32_t RequestedVersion);
typedef uint64_t (*KdpGetHypervisorVendorIdFn)(void);

typedef struct {
    uint32_t FunctionCount;
    KdpExtensibilityExports *Exports;
    KdpSetPciDataByOffsetFn GetPciDataByOffset;
    KdpGetPciDataByOffsetFn SetPciDataByOffset;
    KdpGetPhysicalAddressFn GetPhysicalAddress;
    KdpStallExecutionProcessorFn StallExecutionProcessor;
    KdpReadRegisterUCharFn ReadRegisterUChar;
    KdpReadRegisterUShortFn ReadRegisterUShort;
    KdpReadRegisterULongFn ReadRegisterULong;
    KdpReadRegisterULong64Fn ReadRegisterULong64;
    KdpWriteRegisterUCharFn WriteRegisterUChar;
    KdpWriteRegisterUShortFn WriteRegisterUShort;
    KdpWriteRegisterULongFn WriteRegisterULong;
    KdpWriteRegisterULong64Fn WriteRegisterULong64;
    KdpReadPortUCharFn ReadPortUChar;
    KdpReadPortUShortFn ReadPortUShort;
    KdpReadPortULongFn ReadPortULong;
    KdpReadPortULong64Fn ReadPortULong64;
    KdpWritePortUCharFn WritePortUChar;
    KdpWritePortUShortFn WritePortUShort;
    KdpWritePortULongFn WritePortULong;
    KdpWritePortULong64Fn WritePortULong64;
    KdpSetHiberRangeFn SetHiberRange;
    KdpBugCheckExFn BugCheckEx;
    KdpMapPhysicalMemoryFn MapPhysicalMemory;
    KdpUnmapVirtualAddressFn UnmapVirtualAddress;
    KdpReadCycleCounterFn ReadCycleCounter;
    KdpPrintfFn Printf;
    KdpVmbusInitializeFn VmbusInitialize;
    KdpGetHypervisorVendorIdFn GetHypervisorVendorId;
    uint32_t ExecutionEnvironment;
    uint32_t *ErrorStatus;
    uint16_t **ErrorString;
    uint32_t *HardwareId;
} KdpExtensibilityImports;

typedef uint32_t (*KdpInitializeLibraryFn)(
    KdpExtensibilityImports *ImportTable,
    char *LoaderOptions,
    KdpDebugDeviceDescriptor *Device);

/* Now comes some ethernet-related types. */

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
