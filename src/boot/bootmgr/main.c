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

    char input[] = "A bird came down the walk";
    BmPut("Parsing the input string '%s'\n", input);
    char *token = strtok(input, " ");
    while (token) {
        BmPut("%s\n", token);
        token = strtok(NULL, " ");
    }

    BmPut("Contents of the input string now: '");
    for (size_t n = 0; n < sizeof input; ++n)
        input[n] ? BmPut("%c", input[n]) : BmPut("\\0");
    BmPut("'");

    while (1)
        ;
}
