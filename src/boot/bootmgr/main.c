/* SPDX-FileCopyrightText: (C) 2023 yuuma03
 * SPDX-License-Identifier: BSD-3-Clause */

#include <boot.h>
#include <stdlib.h>
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

    const size_t n = 3;

    const char *src = "Replica";
    char *dup = strndup(src, n);
    BmPut("strndup(\"%s\", %d) == \"%s\" (0x%x)\n", src, n, dup, dup);
    free(dup);

    src = "Hi";
    dup = strndup(src, n);
    BmPut("strndup(\"%s\", %d) == \"%s\" (0x%x)\n", src, n, dup, dup);
    free(dup);

    const char arr[] = {'A', 'B', 'C', 'D'};  // NB: no trailing '\0'
    dup = strndup(arr, n);
    BmPut("strndup({'A','B','C','D'}, %d) == \"%s\" (0x%x)\n", n, dup, dup);
    free(dup);

    while (1)
        ;
}
