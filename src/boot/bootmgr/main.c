/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <boot.h>
#include <display.h>
#include <keyboard.h>
#include <memory.h>
#include <registry.h>
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
    BmInitKeyboard();
    BmInitMemory(BootBlock);
    BmInitArch(BootBlock);
    BmInitStdio();

    do {
        RegHandle *Handle = BmLoadRegistry("boot()/bootmgr.reg");
        if (!Handle) {
            printf("Couldn't find the system registry!");
            break;
        }

        RegEntryHeader *Entries = BmFindRegistryEntry(Handle, NULL, "Entries");
        if (!Entries) {
            printf("Couldn't find the entry list");
            fclose(Handle->Stream);
            free(Handle);
            break;
        }

        int Which = 0;
        while (1) {
            RegEntryHeader *Entry = BmGetRegistryEntry(Handle, Entries, Which++);

            if (!Entry) {
                break;
            } else if (Entry->Type != REG_ENTRY_KEY) {
                continue;
            }

            printf("Entry: %s\n", Entry + 1);
            free(Entry);
        }

        free(Entries);
        fclose(Handle->Stream);
        free(Handle);
    } while (0);

    while (1)
        ;
}
