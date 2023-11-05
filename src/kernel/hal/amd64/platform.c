/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <amd64/halp.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function runs any early arch-specific initialization routines.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpInitializePlatform(void) {
    HalpInitializeIdt();
    HalpInitializeIoapic();
    HalpInitializeApic();
}
