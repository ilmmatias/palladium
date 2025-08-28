/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/kd.h>
#include <kernel/ke.h>

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
    KdPrint(KD_TYPE_INFO, "crashing the system...\n");
    KeFatalError(
        KE_PANIC_MANUALLY_INITIATED_CRASH,
        0xDEADBEEFDEADBEEF,
        0xDEADBEEFDEADBEEF,
        0xDEADBEEFDEADBEEF,
        0xDEADBEEFDEADBEEF);
}
