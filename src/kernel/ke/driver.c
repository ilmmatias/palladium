/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <ki.h>
#include <mi.h>
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
 *     LoaderBlock - Data prepared by the boot loader for us.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KiSaveBootStartDrivers(KiLoaderBlock *LoaderBlock) {
    RtDList *LoaderModuleListHead =
        MmMapSpace((uint64_t)LoaderBlock->BootDriverListHead, sizeof(RtDList));
    if (!LoaderModuleListHead) {
        VidPrint(VID_MESSAGE_ERROR, "Kernel", "couldn't map the boot driver list head\n");
        KeFatalError(KE_PANIC_INSTALL_MORE_MEMORY);
    }

    RtInitializeDList(&KeModuleListHead);
    RtDList *ListHeader = LoaderModuleListHead->Next;

    while (ListHeader != LoaderBlock->BootDriverListHead) {
        ListHeader = MmMapSpace((uint64_t)ListHeader, sizeof(KeModule));
        if (!ListHeader) {
            VidPrint(VID_MESSAGE_ERROR, "Kernel", "couldn't map a boot driver list entry\n");
            KeFatalError(KE_PANIC_INSTALL_MORE_MEMORY);
        }

        KeModule *SourceModule = CONTAINING_RECORD(ListHeader, KeModule, ListHeader);
        KeModule *TargetModule = MmAllocatePool(sizeof(KeModule), "KeLd");
        if (!TargetModule) {
            VidPrint(VID_MESSAGE_ERROR, "Kernel", "couldn't allocate space for a kernel module\n");
            KeFatalError(KE_PANIC_INSTALL_MORE_MEMORY);
        }

        /* Is it safe to assume this is never going to be >1 page long? I hope so, make this
         * a bit more dynamic if we ever start encountering such cases. */
        char *SourceImageName = MmMapSpace((uint64_t)SourceModule->ImageName, MM_PAGE_SIZE);
        if (!SourceImageName) {
            VidPrint(VID_MESSAGE_ERROR, "Kernel", "couldn't map a boot driver list entry\n");
            KeFatalError(KE_PANIC_INSTALL_MORE_MEMORY);
        }

        char *TargetImageName = MmAllocatePool(strlen(SourceImageName) + 1, "KeLd");
        if (!TargetImageName) {
            VidPrint(VID_MESSAGE_ERROR, "Kernel", "couldn't allocate space for a kernel module\n");
            KeFatalError(KE_PANIC_INSTALL_MORE_MEMORY);
        }

        memcpy(TargetModule, SourceModule, sizeof(KeModule));
        strcpy(TargetImageName, SourceImageName);
        TargetModule->ImageName = TargetImageName;
        RtAppendDList(&KeModuleListHead, &TargetModule->ListHeader);

        RtDList *Next = ListHeader->Next;
        MmUnmapSpace(SourceImageName);
        MmUnmapSpace(ListHeader);

        ListHeader = Next;
    }

    MmUnmapSpace(LoaderModuleListHead);
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
    RtDList *ListHeader = KeModuleListHead.Next->Next;
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

        if (Offset < (uint64_t)Image->ImageBase ||
            Offset >= (uint64_t)Image->ImageBase + Image->SizeOfImage) {
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

    uint64_t Start = (uint64_t)Image->ImageBase + *(uint16_t *)(Image->ImageBase + 0x3C);
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

    char OffsetString[23];
    sprintf(OffsetString, "0x%016llx - ", Offset);
    VidPutString(OffsetString);

    VidPutString(Image->ImageName);
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
