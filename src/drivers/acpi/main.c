/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <ke.h>
#include <mm.h>
#include <stdio.h>
#include <stdlib.h>
#include <vid.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function is the entry point of the ACPI compatibility module. We're responsible for
 *     getting things ready so that other drivers can use ACPI functions (such as routing PCI
 *     IRQs).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void DriverEntry(void) {
}
