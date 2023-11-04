/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _KI_H_
#define _KI_H_

#include <ke.h>

#define KI_ACPI_NONE 0
#define KI_ACPI_RDST 1
#define KI_ACPI_XSDT 2

void KiSaveAcpiData(void *LoaderData);
void KiInitializePlatform(void);
void KiRunBootStartDrivers(void *LoaderData);

#endif /* _KI_H_ */
