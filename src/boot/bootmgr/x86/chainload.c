/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <ctype.h>
#include <display.h>
#include <file.h>
#include <config.h>
#include <stdio.h>

extern uint8_t BiosBootDisk;

[[noreturn]] void BiJumpChainload(uint8_t BootDrive);

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function chainloads the given disk/file, making the environment as if the BIOS loaded
 *     it.
 *
 * PARAMETERS:
 *     Entry - Menu entry data containing the full path of the disk/file.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void BiLoadChainload(BmMenuEntry *Entry) {
    BmFile *Handle = BmOpenFile(Entry->Chainload.Path);
    if (!Handle) {
        BmPrint("Could not open the disk or file to be chainloaded.\n"
                "You might need to repair your installation.\n");
        while (1)
            ;
    }

    /* We're either boot(), in which case we'll already have the boot drive on the variable, or
       bios(N), in which case we can just sscanf(). */
    uint8_t BootDrive = BiosBootDisk;
    if (tolower(Entry->Chainload.Path[1]) == 'i') {
        sscanf(Entry->Chainload.Path, "bios(%x)", &BootDrive);
    }

    /* 7C00h should be free for us to use it. */
    if (!BmReadFile(Handle, 0, Handle->Size < 512 ? Handle->Size : 512, (void*)0x7C00)) {
        BmPrint("Could not read the disk or file to be chainloaded.\n"
                "You might need to repair your installation.\n");
        while (1)
            ;
    }

    /* Enter assembly land to wrap up configuring the environemnt.
       This should put us in real mode, go back to text mode, setup DL, and jump to 07C0:0000h. */
    BiJumpChainload(BootDrive);
}
