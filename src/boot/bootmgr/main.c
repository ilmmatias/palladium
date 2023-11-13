/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <boot.h>
#include <config.h>
#include <display.h>
#include <file.h>
#include <ini.h>
#include <keyboard.h>
#include <loader.h>
#include <memory.h>
#include <menu.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function is the BOOTMGR architecture-independent entry point.
 *     We detect and initialize all required hardware, show the boot menu, load up the OS, and
 *     transfer control to it.
 *
 * PARAMETERS:
 *     BootInfo - Information obtained while setting up the boot environment.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void BiMain(void *BootInfo) {
    /* Get the display and memory manager up and running (everything else depends on them). */
    BiZeroRequiredSections();
    BiInitializeDisplay();
    BiReserveLoaderSections();
    BiInitializeMemory();
    BiCalculateMemoryLimits();

    /* Initialize any event related subsystems. */
    BiInitializeKeyboard();

    /* Initialize the filesystem manager, and load up the bootmgr config file. */
    BiInitializeDisks(BootInfo);
    BiLoadConfig();

    /* Finish initializing the platform, and give control to the menu manager. */
    BiInitializePlatform();
    BiInitializeMenu();
}
