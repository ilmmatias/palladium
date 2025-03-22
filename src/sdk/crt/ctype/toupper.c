/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function converts lowercase characters into uppercase, leaving anything else
 *     unchanged.
 *
 * PARAMETERS:
 *     c - Character to convert.
 *
 * RETURN VALUE:
 *     Result of the conversion.
 *-----------------------------------------------------------------------------------------------*/
int toupper(int c) {
    if ((unsigned int)c - 0x61 < 0x1A) {
        c -= 0x20;
    }

    return c;
}
