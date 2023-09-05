/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _KE_H_
#define _KE_H_

#include <stdint.h>

#define KI_ACPI_RDST 0
#define KI_ACPI_XSDT 1

#define KE_FATAL_ERROR 0
#define KE_CORRUPTED_HARDWARE_STRUCTURES 1
#define KE_EARLY_MEMORY_FAILURE 2

void KiSaveAcpiData(void *LoaderData);
uint64_t KiGetAcpiBaseAddress(void);
int KiGetAcpiTableType(void);

void KiRunBootStartDrivers(void *LoaderData);

[[noreturn]] void KeFatalError(int Message);

#endif /* _KE_H_ */
