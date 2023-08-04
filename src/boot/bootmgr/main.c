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

    do {
        FILE *File = fopen("bios()/open_this/flag.txt", "r");
        if (!File) {
            printf("fopen() failed\n");
            break;
        }

        char FileData[54];
        if (!fread(FileData, sizeof(FileData), 1, File)) {
            printf("fread() failed\n");
            break;
        }

        printf("%.*s\n", sizeof(FileData), FileData);
        fclose(File);
    } while (0);

    while (1)
        ;
}
