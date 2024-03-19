/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _SDT_HXX_
#define _SDT_HXX_

#include <stdint.h>

struct __attribute__((packed)) GenericAddressStructure {
    uint8_t AddressSpace;
    uint8_t BitWidth;
    uint8_t BitOffset;
    uint8_t AccessSize;
    uint64_t Address;
};

struct __attribute__((packed)) SdtHeader {
    char Signature[4];
    uint32_t Length;
    uint8_t Revision;
    uint8_t Checksum;
    char OemId[6];
    char OemTableId[8];
    uint32_t OemRevision;
    uint32_t CreatorId;
    uint32_t CreatorRevision;
};

struct __attribute__((packed)) FadtHeader {
    SdtHeader Header;
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
    GenericAddressStructure ResetReg;
    uint8_t ResetValue;
    uint8_t Reserved3[3];
    uint64_t XFirmwareControl;
    uint64_t XDsdt;
    GenericAddressStructure XPm1aEventBlock;
    GenericAddressStructure XPm1bEventBlock;
    GenericAddressStructure XPm1aControlBlock;
    GenericAddressStructure XPm1bControlBlock;
    GenericAddressStructure XPm2ControlBlock;
    GenericAddressStructure XPmTimerBlock;
    GenericAddressStructure XGpe0Block;
    GenericAddressStructure XGpe1Block;
};

#endif /* _SDT_H_ */
