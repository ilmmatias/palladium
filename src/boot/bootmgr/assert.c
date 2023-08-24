/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <display.h>
#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This functions implements the code to notify the user and halt the system on an
 *     unrecoverable error.
 *
 * PARAMETERS:
 *     Message - What error message to display before halting; It'll be displayed at 0;0 in the
 *               screen.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void BmPanic(const char *Message) {
    BmSetColor(DISPLAY_COLOR_PANIC);
    BmInitDisplay();
    BmPutString(Message);
    while (1)
        ;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Internal assertion failed routine, do not call directly! Shows what happened and halts the
 *     system after an expected condition evaluated to false.
 *
 * PARAMETERS:
 *     condition - Which condition failed to evaluate.
 *     file - Which file the assert() was called.
 *     line - Line within the file.
 *     func - Function within the file.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void __assert(const char *condition, const char *file, int line, const char *func) {
    BmSetColor(DISPLAY_COLOR_PANIC);
    BmInitDisplay();
    printf("%s:%d: %s: Assertion `%s` failed.", file, line, func, condition);
    while (1)
        ;
}
