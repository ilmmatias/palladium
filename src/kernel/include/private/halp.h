/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _HALP_H_
#define _HALP_H_

#include <hal.h>
#include <rt.h>

#define HALP_ACPI_NONE 0
#define HALP_ACPI_RDST 1
#define HALP_ACPI_XSDT 2

extern uint32_t HalpProcessorCount;

void HalpSaveAcpiData(void *LoaderData);
void HalpInitializePlatform(int IsBsp, void *Processor);

void HalpSetEvent(uint64_t Time);

#endif /* _HALP_H_ */
