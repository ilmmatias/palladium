/* SPDX-FileCopyrightText: (C) 2023 yuuma03
 * SPDX-License-Identifier: BSD-3-Clause */

#include <boot.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function is the BOOTMGR architecture-independent entry point.
 *     We detect and initialize all required hardware, show the boot menu, load up the OS, and
 *     transfer control to it.
 *
 * PARAMETERS:
 *     BootBlock - Information obtained while setting up the boot environment.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void BmMain(void *BootBlock) {
    BmInitDisplay();
    BmInitArch();
    BmInitMemory(BootBlock);

    char str[50] = "Hello ";
    char str2[50] = "World!";
    strcat(str, str2);
    strncat(str, " Goodbye World!", 3);
    // Expected output is "Hello World! Go"
    BmPut(str);

    while (1)
        ;
}
