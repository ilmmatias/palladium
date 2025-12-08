/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_AMD64_HALPTYPES_H_
#define _KERNEL_DETAIL_AMD64_HALPTYPES_H_

#include <rt/list.h>

typedef union {
    struct __attribute__((packed)) {
        uint64_t Present : 1;
        uint64_t Writable : 1;
        uint64_t User : 1;
        uint64_t WriteThrough : 1;
        uint64_t CacheDisable : 1;
        uint64_t Accessed : 1;
        uint64_t Dirty : 1;
        uint64_t PageSize : 1;
        uint64_t Global : 1;
        uint64_t Available0 : 2;
        uint64_t Pat : 1;
        uint64_t Address : 40;
        uint64_t Available1 : 7;
        uint64_t ProtectionKey : 4;
        uint64_t NoExecute : 1;
    };
    uint64_t RawData;
} HalpPageFrame;

typedef struct __attribute__((packed)) {
    char Signature[4];
    uint32_t Length;
    uint8_t Revision;
    uint8_t Checksum;
    char Unused[26];
} HalpSdtHeader;

typedef struct __attribute__((packed)) {
    uint8_t AddressSpace;
    uint8_t BitWidth;
    uint8_t BitOffset;
    uint8_t AccessSize;
    uint64_t Address;
} HalpFadtGenericAddressStructure;

typedef struct __attribute__((packed)) {
    HalpSdtHeader Header;
    uint32_t FirmwareCtrl;
    uint32_t Dsdt;
    uint8_t Reserved;
    uint8_t PreferredPowerManagementProfile;
    uint16_t SciInterrupt;
    uint32_t SmiCommandPort;
    uint8_t AcpiEnable;
    uint8_t AcpiDisable;
    uint8_t S4BiosReq;
    uint8_t PStateControl;
    uint32_t Pm1aEventBlock;
    uint32_t Pm1bEventBlock;
    uint32_t Pm1aControlBlock;
    uint32_t Pm1bControlBlock;
    uint32_t Pm2ControlBlock;
    uint32_t PmTimerBlock;
    uint32_t Gpe0Block;
    uint32_t Gpe1Block;
    uint8_t Pm1EventLength;
    uint8_t Pm1ControlLength;
    uint8_t Pm2ControlLength;
    uint8_t PmTimerLength;
    uint8_t Gpe0Length;
    uint8_t Gpe1Length;
    uint8_t Gpe1Base;
    uint8_t CStateControl;
    uint16_t WorstC2Latency;
    uint16_t WorstC3Latency;
    uint16_t FlushSize;
    uint16_t FlushStride;
    uint8_t DutyOffset;
    uint8_t DutyWidth;
    uint8_t DayAlarm;
    uint8_t MonthAlarm;
    uint8_t Century;
    uint16_t BootArchitectureFlags;
    uint8_t Reserved2;
    uint32_t Flags;
    HalpFadtGenericAddressStructure ResetReg;
    uint8_t ResetValue;
    uint8_t Reserved3[3];
    uint64_t XFirmwareControl;
    uint64_t XDsdt;
    HalpFadtGenericAddressStructure XPm1aEventBlock;
    HalpFadtGenericAddressStructure XPm1bEventBlock;
    HalpFadtGenericAddressStructure XPm1aControlBlock;
    HalpFadtGenericAddressStructure XPm1bControlBlock;
    HalpFadtGenericAddressStructure XPm2ControlBlock;
    HalpFadtGenericAddressStructure XPmTimerBlock;
    HalpFadtGenericAddressStructure XGpe0Block;
    HalpFadtGenericAddressStructure XGpe1Block;
} HalpFadtHeader;

typedef struct __attribute__((packed)) {
    char Signature[4];
    uint32_t Length;
    char Unused[28];
    uint32_t LapicAddress;
    uint32_t Flags;
} HalpMadtHeader;

typedef struct __attribute__((packed)) {
    uint8_t Type;
    uint8_t Length;
    union {
        struct __attribute__((packed)) {
            uint8_t AcpiId;
            uint8_t ApicId;
            uint32_t Flags;
        } Lapic;
        struct __attribute__((packed)) {
            uint8_t IoapicId;
            uint8_t Reserved;
            uint32_t Address;
            uint32_t GsiBase;
        } Ioapic;
        struct __attribute__((packed)) {
            uint8_t BusSource;
            uint8_t IrqSource;
            uint32_t Gsi;
            uint16_t Flags;
        } IoapicSourceOverride;
        struct __attribute__((packed)) {
            uint16_t Reserved;
            uint64_t Address;
        } LapicAddressOverride;
        struct __attribute__((packed)) {
            uint16_t Reserved;
            uint32_t X2ApicId;
            uint32_t Flags;
            uint32_t AcpiId;
        } X2Apic;
    };
} HalpMadtRecord;

typedef union {
    struct __attribute__((packed)) {
        uint32_t Vector : 8;
        uint32_t Enable : 1;
        uint32_t Reserved0 : 3;
        uint32_t EoiBroadcastDisable : 1;
    };
    uint32_t RawData;
} HalpApicSpivRegister;

typedef union {
    struct __attribute__((packed)) {
        uint32_t Vector : 8;
        uint32_t DeliveryMode : 3;
        uint32_t DestinationMode : 1;
        uint32_t DeliveryStatus : 1;
        uint32_t Reserved0 : 1;
        uint32_t Level : 1;
        uint32_t TriggerMode : 1;
        uint32_t Reserved1 : 2;
        uint32_t DestinationType : 2;
        uint32_t Reserved2 : 12;
        union {
            struct __attribute__((packed)) {
                uint32_t Reserved3 : 24;
                uint32_t DestinationShort : 8;
            };
            uint32_t DestinationFull;
        };
    };
    struct __attribute__((packed)) {
        uint32_t LowData;
        uint32_t HighData;
    };
    uint64_t RawData;
} HalpApicCommandRegister;

typedef union {
    struct __attribute__((packed)) {
        uint32_t Vector : 8;
        uint32_t DeliveryMode : 3;
        uint32_t Reserved0 : 1;
        uint32_t DeliveryStatus : 1;
        uint32_t Polarity : 1;
        uint32_t Remote : 1;
        uint32_t TriggerMode : 1;
        uint32_t Masked : 1;
        uint32_t Periodic : 1;
        uint32_t Reserved1 : 14;
    };
    uint32_t RawData;
} HalpApicLvtRecord;

typedef struct {
    RtSList ListHeader;
    uint32_t ApicId;
    uint32_t AcpiId;
    bool IsX2Apic;
} HalpLapicEntry;

typedef struct {
    RtSList ListHeader;
    uint8_t Id;
    uint32_t GsiBase;
    uint8_t Size;
    char *VirtualAddress;
} HalpIoapicEntry;

typedef struct {
    RtSList ListHeader;
    uint8_t Irq;
    uint8_t Gsi;
    int PinPolarity;
    int TriggerMode;
} HalpIoapicOverrideEntry;

#endif /* _KERNEL_DETAIL_AMD64_HALPTYPES_H_ */
