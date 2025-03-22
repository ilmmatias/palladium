/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function converts uppercase characters into lowercase, leaving anything else
 *     unchanged.
 *
 * PARAMETERS:
 *     c - Character to convert.
 *
 * RETURN VALUE:
 *     Result of the conversion.
 *-----------------------------------------------------------------------------------------------*/
int tolower(int c) {
    if ((unsigned int)c - 0x41 < 0x1A) {
        c += 0x20;
    }

    return c;
}
