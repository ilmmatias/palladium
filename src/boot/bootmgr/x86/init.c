/* SPDX-FileCopyrightText: (C) 2023 yuuma03
 * SPDX-License-Identifier: BSD-3-Clause */

#include <bios.h>
#include <boot.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets up any architecture-dependent features, and gets the system ready for
 *     going further in the boot process.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BmInitArch() {
    for (int i = 0; i < 0x100; i++) {
        BiosRegisters Registers;
        memset(&Registers, 0, sizeof(BiosRegisters));
        BiosCall(0x13, &Registers);
        BmPut("Called it %d time(s).\n", i);
    }
}
