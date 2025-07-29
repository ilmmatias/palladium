/* SPDX-FileCopyrightText: (C) 2024-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _ACPI_H_
#define _ACPI_H_

#include <efi/spec.h>
#include <stdint.h>

typedef struct __attribute__((packed)) {
    char Signature[8];
    uint8_t Checksum;
    char OemId[6];
    uint8_t Revision;
    uint32_t RsdtAddress;
    uint32_t Length;
    uint64_t XsdtAddress;
    uint8_t ExtendedChecksum;
    uint8_t Reserved[3];
} RsdpHeader;

typedef struct __attribute__((packed)) {
    char Signature[4];
    uint32_t Length;
    uint8_t Revision;
    uint8_t Checksum;
    char Unused[26];
} SdtHeader;

bool OslpInitializeAcpi(
    void **AcpiRootPointer,
    void **AcpiRootTable,
    uint32_t *AcpiRootTableSize,
    uint32_t *AcpiVersion);

#endif /* _ACPI_H_ */
