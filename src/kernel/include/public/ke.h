/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _KE_H_
#define _KE_H_

#include <stdint.h>

#define KI_ACPI_NONE 0
#define KI_ACPI_RDST 1
#define KI_ACPI_XSDT 2

#define KE_FATAL_ERROR 0
#define KE_BAD_ACPI_TABLES 1
#define KE_BAD_POOL_HEADER 2
#define KE_DOUBLE_POOL_FREE 3
#define KE_OUT_OF_MEMORY 4

uint64_t KiGetAcpiBaseAddress(void);
int KiGetAcpiTableType(void);

[[noreturn]] void KeFatalError(int Message);

#endif /* _KE_H_ */
