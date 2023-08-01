/* SPDX-FileCopyrightText: (C) 2023 yuuma03
 * SPDX-License-Identifier: BSD-3-Clause */

#include <boot.h>
#include <stdlib.h>

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

    const char *p = "10 200000000000000000000000000000 30 -40 - 42";
    BmPut("Parsing '%s':\n", p);
    char *end = NULL;
    for (unsigned long i = strtoul(p, &end, 10); p != end; i = strtoul(p, &end, 10)) {
        char c = *end;
        *end = 0;
        BmPut("'%s' -> %d\n", p, i);
        *end = c;
        p = end;
    }
    BmPut("After the loop p points to '%s'\n", p);

    while (1)
        ;
}
