/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks if a character is a hexadecimal digit (0-9, aA-fF).
 *
 * PARAMETERS:
 *     c - Character to check.
 *
 * RETURN VALUE:
 *     Result of the check.
 *-----------------------------------------------------------------------------------------------*/
int isxdigit(int c) {
    return (unsigned int)c - 0x30 < 0x0A || (unsigned int)c - 0x41 < 0x06 ||
           (unsigned char)c - 0x61 < 0x06;
}
