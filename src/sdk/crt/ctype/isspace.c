/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks if a character is a white-space.
 *
 * PARAMETERS:
 *     ch - Character to check.
 *
 * RETURN VALUE:
 *     Result of the check.
 *-----------------------------------------------------------------------------------------------*/
int isspace(int ch) {
    return (unsigned int)ch - 0x09 < 0x05 || ch == 0x20;
}
