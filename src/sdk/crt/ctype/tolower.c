/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function converts uppercase characters into lowercase, leaving anything else
 *     unchanged.
 *
 * PARAMETERS:
 *     ch - Character to convert.
 *
 * RETURN VALUE:
 *     Result of the conversion.
 *-----------------------------------------------------------------------------------------------*/
int tolower(int ch) {
    if ((unsigned int)ch - 0x41 < 0x1A) {
        ch += 0x20;
    }

    return ch;
}
