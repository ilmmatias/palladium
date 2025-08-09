/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdint.h>

uintptr_t __stack_chk_guard = (uintptr_t)0xEB976538ECF0;

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
    /* TODO: Add a header containing the valid fast fail codes. */
    __fastfail(0);
}
