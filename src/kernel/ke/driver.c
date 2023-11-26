/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <boot.h>
#include <ke.h>
#include <mm.h>
#include <pe.h>
#include <stdio.h>
#include <string.h>
#include <vid.h>

RtDList KeModuleListHead;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function saves up all images the boot loader prepared for us.
 *     We need to do this before allocating any memory.
 *
 * PARAMETERS:
 *     BootData - Data prepared by the boot loader for us.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KiSaveBootStartDrivers(LoaderBootData *BootData) {
    BootLoaderImage *LoadedImages = BootData->Images.Entries;
    uint32_t LoadedImageCount = BootData->Images.Count;

    RtInitializeDList(&KeModuleListHead);

    while (LoadedImageCount--) {
        KeModule *Module = MmAllocatePool(sizeof(KeModule), "KeLd");
        if (!Module) {
            VidPrint(VID_MESSAGE_ERROR, "Kernel", "couldn't allocate space for a kernel module\n");
            KeFatalError(KE_OUT_OF_MEMORY);
        }

        Module->Name = MI_PADDR_TO_VADDR(LoadedImages->Name);
        Module->ImageBase = LoadedImages->VirtualAddress;
        Module->ImageSize = LoadedImages->ImageSize;
        Module->EntryPoint = LoadedImages->EntryPoint;

        RtAppendDList(&KeModuleListHead, &Module->ListHeader);
        LoadedImages++;
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
    RtDList *ListHeader = KeModuleListHead.Next;
    if (!ListHeader) {
        return;
    }

    ListHeader = ListHeader->Next;
    while (ListHeader != &KeModuleListHead) {
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

    RtDList *ListHeader = KeModuleListHead.Next;
    KeModule *Image = NULL;

    while (ListHeader != &KeModuleListHead) {
        Image = CONTAINING_RECORD(ListHeader, KeModule, ListHeader);

        if (Offset < Image->ImageBase || Offset >= Image->ImageBase + Image->ImageSize) {
            ListHeader = ListHeader->Next;
            continue;
        }

        break;
    }

    if (ListHeader == &KeModuleListHead) {
        char OffsetString[25];
        sprintf(OffsetString, "0x%016llx - ??\n", Offset);
        VidPutString(OffsetString);
        return;
    }

    uint64_t Start = Image->ImageBase + *(uint16_t *)(Image->ImageBase + 0x3C);
    PeHeader *Header = (PeHeader *)Start;
    PeSectionHeader *Sections = (PeSectionHeader *)(Start + Header->SizeOfOptionalHeader + 24);

    /* Clang seems to not strip the coff symbol table out, so we can use it. */
    CoffSymbol *SymbolTable = (CoffSymbol *)(Image->ImageBase + Header->PointerToSymbolTable);
    char *Strings = (char *)(SymbolTable + Header->NumberOfSymbols);
    CoffSymbol *Symbol = SymbolTable;
    CoffSymbol *Closest = NULL;
    uint64_t ClosestAddress = 0;

    while (Symbol < (CoffSymbol *)Strings) {
        if (!Symbol->SectionNumber || Symbol->SectionNumber > Header->NumberOfSections) {
            Symbol += Symbol->NumberOfAuxSymbols + 1;
            continue;
        }

        uint64_t Address =
            Image->ImageBase + Sections[Symbol->SectionNumber - 1].VirtualAddress + Symbol->Value;

        if (Address <= Offset && (!Closest || Offset - Address < Offset - ClosestAddress)) {
            Closest = Symbol;
            ClosestAddress = Address;
        }

        if (Address == Offset) {
            break;
        }

        Symbol += Symbol->NumberOfAuxSymbols + 1;
    }

    char OffsetString[23];
    sprintf(OffsetString, "0x%016llx - ", Offset);
    VidPutString(OffsetString);

    VidPutString(Image->Name);
    VidPutString("!");

    if (!Closest->Name[0] && !Closest->Name[1] && !Closest->Name[2] && !Closest->Name[3]) {
        VidPutString(Strings + *((uint32_t *)Closest + 1));
    } else {
        for (int i = 0; i < 8 && Closest->Name[i]; i++) {
            VidPutChar(Closest->Name[i]);
        }
    }

    if (Offset - ClosestAddress) {
        sprintf(OffsetString, "+%#llx\n", Offset - ClosestAddress);
        VidPutString(OffsetString);
    }
}
