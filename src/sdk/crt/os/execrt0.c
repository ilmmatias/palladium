/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stddef.h>

extern int main(int argc, char *argv[]);

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function serves as the entry point for when the OS transfer execution into a newly
 *     loaded program.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void ExeMainCrtStartup(void) {
    main(0, NULL);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Wrapper around the EXE entry point with a different name.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void mainCRTStartup(void) {
    ExeMainCrtStartup();
}
