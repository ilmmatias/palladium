/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

void KePlaceholder(const char *Message);

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets up the state required for handling new requests, and registers itself
 *     with the kernel.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void DrvMain(void) {
    KePlaceholder("Reached DrvMain() in ntfs.sys");
}
