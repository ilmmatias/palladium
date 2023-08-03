/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks if a character is a lowercase letter.
 *
 * PARAMETERS:
 *     ch - Character to check.
 *
 * RETURN VALUE:
 *     Result of the check.
 *-----------------------------------------------------------------------------------------------*/
int islower(int ch) {
    return (unsigned int)ch - 0x61 < 0x1A;
}
