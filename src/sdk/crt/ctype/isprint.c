/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks if a character is printable (even if it has no graphical
 *     representation; aka, a space).
 *
 * PARAMETERS:
 *     ch - Character to check.
 *
 * RETURN VALUE:
 *     Result of the check.
 *-----------------------------------------------------------------------------------------------*/
int isprint(int ch) {
    return (unsigned int)ch - 0x20 < 0x5F;
}
