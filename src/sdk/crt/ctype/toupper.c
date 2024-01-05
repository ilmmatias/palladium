/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function converts lowercase characters into uppercase, leaving anything else
 *     unchanged.
 *
 * PARAMETERS:
 *     ch - Character to convert.
 *
 * RETURN VALUE:
 *     Result of the conversion.
 *-----------------------------------------------------------------------------------------------*/
int toupper(int ch) {
    if ((unsigned int)ch - 0x61 < 0x1A) {
        ch -= 0x20;
    }

    return ch;
}
