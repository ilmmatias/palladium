/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _AMD64_APIC_H_
#define _AMD64_APIC_H_

#include <rt/list.h>

#define IOAPIC_INDEX 0x00
#define IOAPIC_DATA 0x10

#define IOAPIC_ID_REG 0x00
#define IOAPIC_VER_REG 0x01
#define IOAPIC_ARB_REG 0x02
#define IOAPIC_REDIR_REG_LOW(n) (0x10 + ((n) << 1))
#define IOAPIC_REDIR_REG_HIGH(n) (0x11 + ((n) << 1))

#define LAPIC_RECORD 0
#define IOAPIC_RECORD 1
#define IOAPIC_SOURCE_OVERRIDE_RECORD 2
#define X2APIC_RECORD 9

typedef struct __attribute__((packed)) {
    char Signature[4];
    uint32_t Length;
    char Unused[28];
    uint32_t LapicAddress;
    uint32_t Flags;
} MadtHeader;

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
} MadtRecord;

typedef struct {
    RtSList ListHeader;
    uint32_t ApicId;
    uint32_t AcpiId;
    int IsX2Apic;
} LapicEntry;

typedef struct {
    RtSList ListHeader;
    uint8_t Id;
    uint32_t GsiBase;
    uint8_t Size;
    char *VirtualAddress;
} IoapicEntry;

typedef struct {
    RtSList ListHeader;
    uint8_t Irq;
    uint8_t Gsi;
    int PinPolarity;
    int TriggerMode;
} IoapicOverrideEntry;

#endif /* _AMD64_APIC_H_ */
