/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <stdint.h>

static int CursorY = 0;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Placeholder for testing; Exported in the .lib file.
 *
 * PARAMETERS:
 *     Message - Message to be written to the current line.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KePlaceholder(const char *Message) {
    uint16_t *Screen = (uint16_t *)0xFFFF8000000B8000 + CursorY++ * 80;

    for (int i = 0; Message[i]; i++) {
        Screen[i] = Message[i] | 0x0700;
    }
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
    KePlaceholder("Hello, World (KeMain)!");
    while (1)
        ;
}
