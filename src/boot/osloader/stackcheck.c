/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <console.h>
#include <efi/spec.h>
#include <stdint.h>

uintptr_t __stack_chk_guard = 0xC54FEB976538ECF0;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles errors emitted by the compiler's stack protector.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void __stack_chk_fail(void) {
    OslPutString("Stack smashing detected.\r\n");
    OslPutString("The boot process cannot continue.\r\n");
    gBS->Exit(gIH, EFI_ABORTED, 0, NULL);
    __builtin_unreachable();
}
