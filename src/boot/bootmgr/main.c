/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <boot.h>
#include <stdio.h>

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
    BmInitMemory(BootBlock);
    BmInitArch(BootBlock);

    int i, j, k;
    char str1[10];

    FILE *stream = fopen("bios()/open_this/flag.txt", "r");
    if (!stream) {
        printf("fopen() failed\n");
        while (1)
            ;
    }

    /* parse as follows:
       %d: an integer
       %9s: a string of at most 9 non-whitespace characters
       %2d: two-digit integer (digits 5 and 6)
       %*d: an integer which isn't stored anywhere
       %n: characters read so far */
    int ret = fscanf(stream, "%d%9s%2d%*d%n", &i, str1, &j, &k);

    printf(
        "Converted %d fields:\n"
        "i = %d\n"
        "str1 = %s\n"
        "j = %d\n"
        "k = %d\n",
        ret,
        i,
        str1,
        j,
        k);

    fclose(stream);
    while (1)
        ;
}
