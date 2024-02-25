/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _ACPI_H_
#define _ACPI_H_

#include <efi/spec.h>
#include <stdint.h>

typedef struct __attribute__((aligned)) {
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

int OslpInitializeAcpi(void **AcpiTable, uint32_t *AcpiTableVersion);

#endif /* _ACPI_H_ */
