/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/ki.h>
#include <kernel/mi.h>
#include <kernel/vid.h>
#include <pe.h>
#include <stdio.h>
#include <string.h>

RtDList KiModuleListHead;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function saves up all images the boot loader prepared for us.
 *     We need to do this before allocating any memory.
 *
 * PARAMETERS:
 *     LoaderBlock - Data prepared by the boot loader for us.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KiSaveBootStartDrivers(KiLoaderBlock *LoaderBlock) {
    RtInitializeDList(&KiModuleListHead);
    for (RtDList *ListHeader = LoaderBlock->BootDriverListHead->Next;
         ListHeader != LoaderBlock->BootDriverListHead;
         ListHeader = ListHeader->Next) {
        KeModule *SourceModule = CONTAINING_RECORD(ListHeader, KeModule, ListHeader);
        KeModule *TargetModule = MmAllocatePool(sizeof(KeModule), "KeLd");
        if (!TargetModule) {
            KeFatalError(
                KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_DRIVER_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
                0,
                0);
        }

        char *TargetImageName = MmAllocatePool(strlen(SourceModule->ImageName) + 1, "KeLd");
        if (!TargetImageName) {
            KeFatalError(
                KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_DRIVER_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
                0,
                0);
        }

        memcpy(TargetModule, SourceModule, sizeof(KeModule));
        strcpy(TargetImageName, SourceModule->ImageName);
        TargetModule->ImageName = TargetImageName;
        RtAppendDList(&KiModuleListHead, &TargetModule->ListHeader);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function runs all the boot start driver entry points.
 *     After this, we should be ready to read more drivers from the disk.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KiRunBootStartDrivers(void) {
    /* The kernel should be the first image, and the drivers start from there onwards. */
    RtDList *ListHeader = KiModuleListHead.Next->Next;
    while (ListHeader != &KiModuleListHead) {
        KeModule *Module = CONTAINING_RECORD(ListHeader, KeModule, ListHeader);
        ((void (*)(void))Module->EntryPoint)();
        ListHeader = ListHeader->Next;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function dumps information about the given symbol, using the data from the loaded
 *     images. This doesn't/shouldn't use any memory, as we're used on the panic function.
 *
 * PARAMETERS:
 *     Address - What we want to get the info of.
 *     Offset - Output; Offset from the found symbol.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KiDumpSymbol(void *Address) {
    uint64_t Offset = (uint64_t)Address;
    RtDList *ListHeader = KiModuleListHead.Next;
    KeModule *Image = NULL;

    while (ListHeader != &KiModuleListHead) {
        Image = CONTAINING_RECORD(ListHeader, KeModule, ListHeader);

        if (Offset < (uint64_t)Image->ImageBase ||
            Offset >= (uint64_t)Image->ImageBase + Image->SizeOfImage) {
            ListHeader = ListHeader->Next;
            continue;
        }

        break;
    }

    if (ListHeader == &KiModuleListHead) {
        VidPrintSimple("0x%016llx - ??\n", Offset);
        return;
    }

    uint64_t Start = (uint64_t)Image->ImageBase + *(uint16_t *)(Image->ImageBase + 0x3C);
    PeHeader *Header = (PeHeader *)Start;
    PeSectionHeader *Sections = (PeSectionHeader *)(Start + Header->SizeOfOptionalHeader + 24);

    /* Clang doesn't strip the coff symbol table out as long as you don't ask it to generate a
     * separate PDB file. */
    CoffSymbol *SymbolTable = (CoffSymbol *)(Image->ImageBase + Header->PointerToSymbolTable);
    char *Strings = (char *)(SymbolTable + Header->NumberOfSymbols);
    CoffSymbol *Symbol = SymbolTable;
    CoffSymbol *Closest = NULL;
    uint64_t ClosestAddress = 0;
    if (!Header->PointerToSymbolTable) {
        VidPrintSimple(
            "0x%016llx - %s+%#llx\n",
            Offset,
            Image->ImageName,
            Offset - (uint64_t)Image->ImageBase);
        return;
    }

    while (Symbol < (CoffSymbol *)Strings) {
        if (!Symbol->SectionNumber || Symbol->SectionNumber > Header->NumberOfSections) {
            Symbol += Symbol->NumberOfAuxSymbols + 1;
            continue;
        }

        uint64_t Address = (uint64_t)Image->ImageBase +
                           Sections[Symbol->SectionNumber - 1].VirtualAddress + Symbol->Value;

        if (Address <= Offset && (!Closest || Offset - Address < Offset - ClosestAddress)) {
            Closest = Symbol;
            ClosestAddress = Address;
        }

        if (Address == Offset) {
            break;
        }

        Symbol += Symbol->NumberOfAuxSymbols + 1;
    }

    VidPrintSimple("0x%016llx - %s!", Offset, Image->ImageName);

    if (!Closest->Name[0] && !Closest->Name[1] && !Closest->Name[2] && !Closest->Name[3]) {
        VidPutString(Strings + *((uint32_t *)Closest + 1));
    } else {
        for (int i = 0; i < 8 && Closest->Name[i]; i++) {
            VidPutChar(Closest->Name[i]);
        }
    }

    VidPrintSimple("+%#llx\n", Offset - ClosestAddress);
}
