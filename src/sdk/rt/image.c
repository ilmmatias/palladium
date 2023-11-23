/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <ke.h>
#include <pe.h>

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
    RtDList *ListHeader = KeModuleListHead.Next;

    while (ListHeader != &KeModuleListHead) {
        KeModule *Module = CONTAINING_RECORD(ListHeader, KeModule, ListHeader);

        if (Address >= Module->ImageBase && Address <= Module->ImageBase + Module->ImageSize) {
            return Module->ImageBase;
        }

        ListHeader = ListHeader->Next;
    }

    return 0;
}
