/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/ke.h>

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
    KeFatalError(KE_PANIC_STACK_SMASHING_DETECTED, 0, 0, 0, 0);
}
