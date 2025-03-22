/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

extern void DllMain(void);

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function serves as the entry point for when a DLL gets attached or detached from a
 *     thread or process.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void DllMainCrtStartup(void) {
    DllMain();
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Wrapper around the DLL entry point with a different name.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void DllMainCRTStartup(void) {
    DllMainCrtStartup();
}
