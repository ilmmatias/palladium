/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/ke.h>
#include <os/pe.h>

extern RtDList KiModuleListHead;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function searches for the base of the PE file the given address belongs to.
 *
 * PARAMETERS:
 *     Address - Which address we're resolving.
 *
 * RETURN VALUE:
 *     Either the base address, or 0 on failure.
 *-----------------------------------------------------------------------------------------------*/
uint64_t RtLookupImageBase(uint64_t Address) {
    RtDList *ListHeader = KiModuleListHead.Next;

    while (ListHeader != &KiModuleListHead) {
        KeModule *Module = CONTAINING_RECORD(ListHeader, KeModule, ListHeader);

        if (Address >= (uint64_t)Module->ImageBase &&
            Address <= (uint64_t)Module->ImageBase + Module->SizeOfImage) {
            return (uint64_t)Module->ImageBase;
        }

        ListHeader = ListHeader->Next;
    }

    return 0;
}
