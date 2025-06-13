/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/os.h>
#include <stdio.h>
#include <stdlib.h>

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
    static const char* str = "stack smashing detected\n";

    if (stderr) {
        size_t wrote;
        __write_file(stderr->handle, str, sizeof(str), &wrote);
    }

    __terminate_process(EXIT_FAILURE);
}
