/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <boot.h>
#include <mm.h>
#include <pe.h>
#include <stdio.h>
#include <vid.h>

static BootLoaderImage *LoadedImages;
static uint32_t LoadedImageCount;

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
    LoadedImages = BootData->Images.Entries;
    LoadedImageCount = BootData->Images.Count;
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
    /* Skipping the first image, as it is certain to be the kernel itself. */
    for (uint32_t i = 1; i < LoadedImageCount; i++) {
        ((void (*)(void))LoadedImages[i].EntryPoint)();
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

    BootLoaderImage *Image = LoadedImages;
    while (Image < LoadedImages + LoadedImageCount) {
        if (Offset < Image->VirtualAddress || Offset >= Image->VirtualAddress + Image->ImageSize) {
            Image++;
            continue;
        }

        break;
    }

    if (Image >= LoadedImages + LoadedImageCount) {
        char OffsetString[25];
        sprintf(OffsetString, "%#llx - ??\n", Offset);
        VidPutString(OffsetString);
        return;
    }

    uint64_t Start = Image->VirtualAddress + *(uint16_t *)(Image->VirtualAddress + 0x3C);
    PeHeader *Header = (PeHeader *)Start;
    PeSectionHeader *Sections = (PeSectionHeader *)(Start + Header->SizeOfOptionalHeader + 24);

    /* Clang seems to not strip the coff symbol table out, so we can use it. */
    CoffSymbol *SymbolTable = (CoffSymbol *)(Image->VirtualAddress + Header->PointerToSymbolTable);
    char *Strings = (char *)(SymbolTable + Header->NumberOfSymbols);
    CoffSymbol *Symbol = SymbolTable;
    CoffSymbol *Closest = NULL;
    uint64_t ClosestAddress = 0;

    while (Symbol < (CoffSymbol *)Strings) {
        if (!Symbol->SectionNumber || Symbol->SectionNumber > Header->NumberOfSections) {
            Symbol += Symbol->NumberOfAuxSymbols + 1;
            continue;
        }

        uint64_t Address = Image->VirtualAddress +
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

    char OffsetString[22];
    sprintf(OffsetString, "%#llx - ", Offset);
    VidPutString(OffsetString);

    VidPutString(MI_PADDR_TO_VADDR(Image->Name));
    VidPutString("!");

    if (!Closest->Name[0] && !Closest->Name[1] && !Closest->Name[2] && !Closest->Name[3]) {
        VidPutString(Strings + *((uint32_t *)Closest + 1));
    } else {
        for (int i = 0; i < 8 && Closest->Name[i]; i++) {
            VidPutChar(Closest->Name[i]);
        }
    }

    sprintf(OffsetString, "+%#llx", Offset - ClosestAddress);
    VidPutString(OffsetString);
}
