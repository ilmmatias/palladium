/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks if a character is a digit.
 *
 * PARAMETERS:
 *     c - Character to check.
 *
 * RETURN VALUE:
 *     Result of the check.
 *-----------------------------------------------------------------------------------------------*/
int isdigit(int c) {
    return (unsigned int)c - 0x30 < 0x0A;
}
