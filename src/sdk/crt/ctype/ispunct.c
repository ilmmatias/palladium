/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks if a character is a punctuation character.
 *
 * PARAMETERS:
 *     c - Character to check.
 *
 * RETURN VALUE:
 *     Result of the check.
 *-----------------------------------------------------------------------------------------------*/
int ispunct(int c) {
    return (unsigned int)c - 0x21 < 0x0F || (unsigned int)c - 0x3A < 0x07 ||
           (unsigned int)c - 0x5B < 0x06 || (unsigned int)c - 0x7B < 0x04;
}
