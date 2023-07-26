/* SPDX-FileCopyrightText: (C) 2023 yuuma03
 * SPDX-License-Identifier: BSD-3-Clause */

#include <boot.h>
#include <stdarg.h>

static void PutString(const char *String) {
    while (*String) {
        BiPutChar(*String++);
    }
}

static void PutHex(uint64_t Number) {
    for (int i = 60; i >= 0; i -= 4) {
        BiPutChar("0123456789ABCDEF"[(Number >> i) & 0xF]);
    }
}

static void PutDec(uint64_t Number) {
    if (Number >= 10) {
        PutDec(Number / 10);
    }

    BiPutChar('0' + Number % 10);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function formats and displays a string. It supports the following format specifiers:
 *         %c - Character
 *         %s - String
 *         %x - Hexadecimal number
 *         %d - Decimal number
 *
 * PARAMETERS:
 *     String - The format string.
 *     ...    - The arguments to be inserted into the format string.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BmPut(const char *String, ...) {
    va_list Args;
    va_start(Args, String);

    while (*String) {
        if (*String == '%') {
            String++;

            switch (*String) {
                case 'c': {
                    BiPutChar(va_arg(Args, int));
                    break;
                }
                case 's': {
                    PutString(va_arg(Args, const char *));
                    break;
                }
                case 'x': {
                    PutHex(va_arg(Args, uint64_t));
                    break;
                }
                case 'd': {
                    PutDec(va_arg(Args, uint64_t));
                    break;
                }
                default: {
                    BiPutChar(*String);
                    break;
                }
            }
        } else {
            BiPutChar(*String);
        }

        String++;
    }

    va_end(Args);
}
