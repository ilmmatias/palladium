/* SPDX-FileCopyrightText: (C) 2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _OS_KDEXT_H_
#define _OS_KDEXT_H_

#include <stdint.h>

/* Sync these with the equivalent defines from the WDK matching the wanted source of the KDNET
 * drivers. */

#define KD_DEVICE_FLAGS_HAL_SCRATCH_ALLOCATED 0x01
#define KD_DEVICE_FLAGS_BARS_MAPPED 0x02
#define KD_DEVICE_FLAGS_SCRATCH_ALLOCATED 0x04
#define KD_DEVICE_FLAGS_UNCACHED_MEMORY 0x08

#define KD_RESOURCE_NULL 0
#define KD_RESOURCE_PORT 1
#define KD_RESOURCE_INTERRUPT 2
#define KD_RESOURCE_MEMORY 3
#define KD_RESOURCE_DMA 4
#define KD_RESOURCE_DEVICE_SPECIFIC 5
#define KD_RESOURCE_BUS_NUMBER 6
#define KD_RESOURCE_MEMORY_LARGE 7

#define KD_NAMESPACE_PCI 0
#define KD_NAMESPACE_ACPI 1
#define KD_NAMESPACE_ANY 2
#define KD_NAMESPACE_NONE 3
#define KD_NAMESPACE_MA 4

#define KD_ENVIRONMENT_UNSPECIFIED 0
#define KD_ENVIRONMENT_BOOT 1
#define KD_ENVIRONMENT_KERNEL 2
#define KD_ENVIRONMENT_HYPERVISOR 3
#define KD_ENVIRONMENT_SK 4
#define KD_ENVIRONMENT_HCL 5

#define KD_EXTENSIBILITY_EXPORT_COUNT 15
#define KD_EXTENSIBILITY_IMPORT_COUNT 33

#define KD_TRANSMIT_LAST 0x20000000
#define KD_TRANSMIT_HANDLE 0x40000000
#define KD_TRANSMIT_ASYNC 0x80000000
#define KD_HANDLE_FLAGS (KD_TRANSMIT_LAST | KD_TRANSMIT_HANDLE | KD_TRANSMIT_ASYNC)

/* And sync these with the equivalent types. */

typedef union {
    struct {
        uint32_t LowPart;
        uint32_t HighPart;
    };
    uint64_t QuadPart;
} KdPhysicalAddress;

typedef struct {
    uint8_t Type;
    bool Valid;
    uint8_t BitWidth;
    uint8_t AccessSize;
    uint8_t *TranslatedAddress;
    uint32_t Length;
} KdDebugDeviceAddress;

typedef struct {
    KdPhysicalAddress Start;
    KdPhysicalAddress MaxEnd;
    void *VirtualAddress;
    uint32_t Length;
    bool Cached;
    bool Aligned;
} KdDebugMemoryRequirements;

typedef struct {
    uint32_t HwContextSize;
    uint32_t SharedVisibleDataSize;
    bool UseSerialFraming;
    bool ValidUsbCoreId;
    uint8_t UsbCoreId;
} KdDebugTransportData;

typedef struct {
    void *PciIoProcotolHandle;
    void *Mapping;
} KdDebugEfiIoMmuData;

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
    KdDebugDeviceAddress BaseAddress[6];
    KdDebugMemoryRequirements Memory;
    uint32_t Dbg2TableIndex;
    uint16_t PortType;
    uint16_t PortSubtype;
    void *OemData;
    uint32_t OemDataLength;
    uint32_t NameSpace;
    uint16_t *NameSpacePath;
    uint32_t NameSpacePathLength;
    uint32_t TransportType;
    KdDebugTransportData TransportData;
    KdDebugEfiIoMmuData EfiIoMmuData;
} KdDebugDeviceDescriptor;

typedef struct {
    void *Hardware;
    KdDebugDeviceDescriptor *Device;
    uint8_t *TargetMacAddress;
    uint32_t LinkSpeed;
    uint32_t LinkDuplex;
    uint8_t *LinkState;
    uint32_t SerialBaudRate;
    uint32_t Flags;
    uint8_t RestartKdnet;
    uint8_t Reserved[3];
} KdExtensibilityData;

typedef uint32_t (*KdInitializeControllerFn)(KdExtensibilityData *KdNet);
typedef void (*KdShutdownControllerFn)(void *Adapter);
typedef void (*KdSetHibernateRangeFn)(void);
typedef uint32_t (
    *KdGetRxPacketFn)(void *Adapter, uint32_t *Handle, void **Packet, uint32_t *Length);
typedef void (*KdReleaseRxPacketFn)(void *Adapter, uint32_t Handle);
typedef uint32_t (*KdGetTxPacketFn)(void *Adapter, uint32_t *Handle);
typedef uint32_t (*KdSendTxPacketFn)(void *Adapter, uint32_t Handle, uint32_t Length);
typedef void *(*KdGetPacketAddressFn)(void *Adapter, uint32_t Handle);
typedef uint32_t (*KdGetPacketLengthFn)(void *Adapter, uint32_t Handle);
typedef uint32_t (*KdGetHardwareContextSizeFn)(KdDebugDeviceDescriptor *Device);
typedef uint32_t (*KdDeviceControlFn)(
    void *Adapter,
    uint32_t RequestCode,
    void *InputBuffer,
    uint32_t InputBufferLength,
    void *OutputBuffer,
    uint32_t OutputBufferLength);
typedef uint32_t (*KdReadSerialByteFn)(void *Adapter, uint8_t *Byte);
typedef uint32_t (*KdWriteSerialByteFn)(void *Adapter, uint8_t Byte);
typedef uint32_t (
    *KdSerialOutputInitFn)(KdDebugDeviceDescriptor *Device, KdPhysicalAddress *Address);
typedef void (*KdSerialOutputByteFn)(uint8_t Byte);

typedef struct {
    uint32_t FunctionCount;
    KdInitializeControllerFn InitializeController;
    KdShutdownControllerFn ShutdownController;
    KdSetHibernateRangeFn SetHibernateRange;
    KdGetRxPacketFn GetRxPacket;
    KdReleaseRxPacketFn ReleaseRxPacket;
    KdGetTxPacketFn GetTxPacket;
    KdSendTxPacketFn SendTxPacket;
    KdGetPacketAddressFn GetPacketAddress;
    KdGetPacketLengthFn GetPacketLength;
    KdGetHardwareContextSizeFn GetHardwareContextSize;
    KdDeviceControlFn DeviceControl;
    KdReadSerialByteFn ReadSerialByte;
    KdWriteSerialByteFn WriteSerialByte;
    KdSerialOutputInitFn SerialOutputInit;
    KdSerialOutputByteFn SerialOutputByte;
} KdExtensibilityExports;

/* I found what seems to be a strange choice when looking at kdnetextensibility.h to build these
 * types: Why is it that Read/WritePortULong64 take uint32_t pointers and return uint32_ts? That
 * doesn't make much sense, but I'll leave it as is. */

typedef uint32_t (*KdGetPciDataByOffsetFn)(
    uint32_t BusNumber,
    uint32_t SlotNumber,
    void *Buffer,
    uint32_t Offset,
    uint32_t Length);
typedef uint32_t (*KdSetPciDataByOffsetFn)(
    uint32_t BusNumber,
    uint32_t SlotNumber,
    void *Buffer,
    uint32_t Offset,
    uint32_t Length);
typedef KdPhysicalAddress (*KdGetPhysicalAddressFn)(void *Va);
typedef void (*KdStallExecutionProcessorFn)(uint32_t Microseconds);
typedef uint8_t (*KdReadRegisterUCharFn)(uint8_t *Register);
typedef uint16_t (*KdReadRegisterUShortFn)(uint16_t *Register);
typedef uint32_t (*KdReadRegisterULongFn)(uint32_t *Register);
typedef uint64_t (*KdReadRegisterULong64Fn)(uint64_t *Register);
typedef void (*KdWriteRegisterUCharFn)(uint8_t *Register, uint8_t Value);
typedef void (*KdWriteRegisterUShortFn)(uint16_t *Register, uint16_t Value);
typedef void (*KdWriteRegisterULongFn)(uint32_t *Register, uint32_t Value);
typedef void (*KdWriteRegisterULong64Fn)(uint64_t *Register, uint64_t Value);
typedef uint8_t (*KdReadPortUCharFn)(uint8_t *Port);
typedef uint16_t (*KdReadPortUShortFn)(uint16_t *Port);
typedef uint32_t (*KdReadPortULongFn)(uint32_t *Port);
typedef uint32_t (*KdReadPortULong64Fn)(uint64_t *Port);
typedef void (*KdWritePortUCharFn)(uint8_t *Port, uint8_t Value);
typedef void (*KdWritePortUShortFn)(uint16_t *Port, uint16_t Value);
typedef void (*KdWritePortULongFn)(uint32_t *Port, uint32_t Value);
typedef void (*KdWritePortULong64Fn)(uint32_t *Port, uint64_t Value);
typedef void (*KdSetHiberRangeFn)(
    void *MemoryMap,
    uint32_t Flags,
    void *Address,
    uint32_t Length,
    uint32_t Tag);
typedef void (*KdBugCheckExFn)(
    uint32_t BugCheckCode,
    uint32_t BugCheckParameter1,
    uint32_t BugCheckParameter2,
    uint32_t BugCheckParameter3,
    uint32_t BugCheckParameter4);
typedef void *(*KdMapPhysicalMemoryFn)(
    KdPhysicalAddress PhysicalAddress,
    uint32_t NumberPages,
    bool FlushCurrentTlb);
typedef void (
    *KdUnmapVirtualAddressFn)(void *VirtualAddress, uint32_t NumberPages, bool FlushCurrentTlb);
typedef uint64_t (*KdReadCycleCounterFn)(uint64_t *Frequency);
typedef void (*KdPrintfFn)(char *Format, ...);
typedef bool (*KdVmbusInitializeFn)(
    void *Context,
    void *Parameters,
    bool UnreserveChannels,
    void *ArrivalRoutine,
    void *ArrivalRoutineContext,
    uint32_t RequestedVersion);
typedef uint64_t (*KdGetHypervisorVendorIdFn)(void);

typedef struct {
    uint32_t FunctionCount;
    KdExtensibilityExports *Exports;
    KdSetPciDataByOffsetFn GetPciDataByOffset;
    KdGetPciDataByOffsetFn SetPciDataByOffset;
    KdGetPhysicalAddressFn GetPhysicalAddress;
    KdStallExecutionProcessorFn StallExecutionProcessor;
    KdReadRegisterUCharFn ReadRegisterUChar;
    KdReadRegisterUShortFn ReadRegisterUShort;
    KdReadRegisterULongFn ReadRegisterULong;
    KdReadRegisterULong64Fn ReadRegisterULong64;
    KdWriteRegisterUCharFn WriteRegisterUChar;
    KdWriteRegisterUShortFn WriteRegisterUShort;
    KdWriteRegisterULongFn WriteRegisterULong;
    KdWriteRegisterULong64Fn WriteRegisterULong64;
    KdReadPortUCharFn ReadPortUChar;
    KdReadPortUShortFn ReadPortUShort;
    KdReadPortULongFn ReadPortULong;
    KdReadPortULong64Fn ReadPortULong64;
    KdWritePortUCharFn WritePortUChar;
    KdWritePortUShortFn WritePortUShort;
    KdWritePortULongFn WritePortULong;
    KdWritePortULong64Fn WritePortULong64;
    KdSetHiberRangeFn SetHiberRange;
    KdBugCheckExFn BugCheckEx;
    KdMapPhysicalMemoryFn MapPhysicalMemory;
    KdUnmapVirtualAddressFn UnmapVirtualAddress;
    KdReadCycleCounterFn ReadCycleCounter;
    KdPrintfFn Printf;
    KdVmbusInitializeFn VmbusInitialize;
    KdGetHypervisorVendorIdFn GetHypervisorVendorId;
    uint32_t ExecutionEnvironment;
    uint32_t *ErrorStatus;
    uint16_t **ErrorString;
    uint32_t *HardwareId;
} KdExtensibilityImports;

typedef uint32_t (*KdInitializeLibraryFn)(
    KdExtensibilityImports *ImportTable,
    char *LoaderOptions,
    KdDebugDeviceDescriptor *Device);

#endif /* _OS_KDEXT_H_  */
