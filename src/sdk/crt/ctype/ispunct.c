/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks if a character is a punctuation character.
 *
 * PARAMETERS:
 *     ch - Character to check.
 *
 * RETURN VALUE:
 *     Result of the check.
 *-----------------------------------------------------------------------------------------------*/
int ispunct(int ch) {
    return (unsigned int)ch - 0x21 < 0x0F || (unsigned int)ch - 0x3A < 0x07 ||
           (unsigned int)ch - 0x5B < 0x06 || (unsigned int)ch - 0x7B < 0x04;
}
