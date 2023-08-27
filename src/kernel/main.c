/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <stdint.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Placeholder for testing; Exported in the .lib file.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KePlaceholder() {
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function is the kernel architecture-independent entry point.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void KeMain(void) {
    const char *Message = "Hello, World!";
    uint16_t *Screen = (uint16_t *)0xFFFF8000000B8000;

    for (int i = 0; Message[i]; i++) {
        Screen[i] = Message[i] | 0x0700;
    }

    while (1)
        ;
}
