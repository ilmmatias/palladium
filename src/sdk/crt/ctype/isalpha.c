/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks if a character is alphabetic (a letter).
 *
 * PARAMETERS:
 *     c - Character to check.
 *
 * RETURN VALUE:
 *     Result of the check.
 *-----------------------------------------------------------------------------------------------*/
int isalpha(int c) {
    return (unsigned int)c - 0x41 < 0x1A || (unsigned int)c - 0x61 < 0x1A;
}
