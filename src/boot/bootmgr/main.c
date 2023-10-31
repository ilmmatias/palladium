/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <boot.h>
#include <display.h>
#include <keyboard.h>
#include <memory.h>
#include <registry.h>

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
[[noreturn]] void BiMain(void *BootBlock) {
    BiInitDisplay();
    BiInitKeyboard();
    BiInitMemory(BootBlock);
    BiInitArch(BootBlock);
    BiInitRegistry();
    BiLoadMenuEntries();
    BiEnterMenu();
}
