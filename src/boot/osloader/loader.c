/* SPDX-FileCopyrightText: (C) 2024-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <console.h>
#include <file.h>
#include <loader.h>
#include <memory.h>
#include <pe.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function loads up the given PE image into memory, and adds it to the loaded programs
 *     list.
 *
 * PARAMETERS:
 *     LoadedPrograms - Header of the loaded programs list.
 *     ImageName - Which name should be associated with this entry.
 *     ImagePath - Full path where the executable is.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int OslLoadExecutable(RtDList *LoadedPrograms, const char *ImageName, const char *ImagePath) {
    OslPrint("loading up %s\r\n", ImagePath);

    /* Preload the entire file (to prevent multiple small reads). */
    uint64_t BufferSize = 0;
    char *Buffer = OslReadFile(ImagePath, &BufferSize);
    if (!Buffer) {
        OslPrint("Failed to open a kernel/driver file.\r\n");
        OslPrint("Couldn't find %s on the boot/root volume.\r\n", ImagePath);
        OslPrint("The boot process cannot continue.\r\n");
        return 0;
    }

    /* The PE data is prefixed with a MZ header and a MS-DOS stub; The offset into the PE data is
       guaranteed to be after the main MZ header. */
    uint32_t Offset = *(uint32_t *)(Buffer + 0x3C);
    PeHeader *Header = (PeHeader *)(Buffer + Offset);

    /* Following the information at https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
       for the implementation. */
    if (memcmp(Header->Signature, PE_SIGNATURE, 4) || Header->Machine != PE_MACHINE ||
        (Header->Characteristics & 0x2000) || Header->Magic != 0x20B || Header->Subsystem != 1 ||
        (Header->DllCharacteristics & 0x160) != 0x160) {
        OslPrint("Failed to load a kernel/driver file.\r\n");
        OslPrint("The file at %s doesn't seem to be a valid for this architecture.\r\n", ImagePath);
        OslPrint("The boot process cannot continue.\r\n");
        return 0;
    }

    /* Calculate the size (and were to put) the symbol table; Loading up all sections to the proper
       places will probably trash it (and the strings table) as we currently stand. */
    char *SourceSymbols = Buffer + Header->PointerToSymbolTable;
    uint32_t TargetSymbols = Header->SizeOfImage;
    uint32_t Strings = *(uint32_t *)(SourceSymbols + Header->NumberOfSymbols * 18);
    uint32_t SymbolTableSize = Strings + Header->NumberOfSymbols * 18;
    uint32_t ImagePages =
        (Header->SizeOfImage + SymbolTableSize + EFI_PAGE_SIZE - 1) >> EFI_PAGE_SHIFT;

    /* Start filling the OslpLoadedProgram structure. */
    OslpLoadedProgram *ThisProgram = NULL;
    EFI_STATUS Status =
        gBS->AllocatePool(EfiLoaderData, sizeof(OslpLoadedProgram), (VOID **)&ThisProgram);
    if (Status != EFI_SUCCESS) {
        OslPrint("Failed to load a kernel/driver file.\r\n");
        OslPrint("The system ran out of memory while loading %s.\r\n", ImagePath);
        OslPrint("The boot process cannot continue.\r\n");
        return 0;
    }

    ThisProgram->Name = ImageName;
    ThisProgram->ImageSize = ImagePages * EFI_PAGE_SIZE;

    Status = gBS->AllocatePool(
        EfiLoaderData, ImagePages * sizeof(int), (VOID **)&ThisProgram->PageFlags);
    if (Status != EFI_SUCCESS) {
        OslPrint("Failed to load a kernel/driver file.\r\n");
        OslPrint("The system ran out of memory while loading %s.\r\n", ImagePath);
        OslPrint("The boot process cannot continue.\r\n");
        return 0;
    }

    ThisProgram->PhysicalAddress =
        OslAllocatePages(ThisProgram->ImageSize, (1 << VIRTUAL_RANDOM_SHIFT));
    if (!ThisProgram->PhysicalAddress) {
        OslPrint("Failed to load a kernel/driver file.\r\n");
        OslPrint("The system ran out of memory while loading %s.\r\n", ImagePath);
        OslPrint("The boot process cannot continue.\r\n");
        return 0;
    }

    ThisProgram->VirtualAddress = OslAllocateVirtualAddress(ImagePages);
    if (!ThisProgram->VirtualAddress) {
        OslPrint("Failed to load a kernel/driver file.\r\n");
        OslPrint("The system ran out of virtual memory while loading %s.\r\n", ImagePath);
        OslPrint("The boot process cannot continue.\r\n");
        return 0;
    }

    /* Cleanup the page flags array. */
    memset(ThisProgram->PageFlags, 0, ImagePages * sizeof(int));

    /* The kernel might use information from the base headers and section headers; SizeOfImage
       should have given us the code/data size + all headers, so we're assuming that and loading
       it all up to the base address. */
    memcpy(ThisProgram->PhysicalAddress, Buffer, Header->SizeOfHeaders);
    memcpy((char *)ThisProgram->PhysicalAddress + TargetSymbols, SourceSymbols, SymbolTableSize);

    uint64_t ExpectedBase = Header->ImageBase;
    Header = (PeHeader *)((char *)ThisProgram->PhysicalAddress + Offset);
    Header->ImageBase = (uint64_t)ThisProgram->VirtualAddress;
    Header->PointerToSymbolTable = TargetSymbols;

    ThisProgram->BaseDiff = (uint64_t)ThisProgram->VirtualAddress - ExpectedBase;
    ThisProgram->EntryPoint =
        (void *)((uint64_t)ThisProgram->VirtualAddress + Header->AddressOfEntryPoint);

    PeSectionHeader *Sections = (PeSectionHeader *)((char *)ThisProgram->PhysicalAddress + Offset +
                                                    Header->SizeOfOptionalHeader + 24);

    for (uint16_t i = 0; i < Header->NumberOfSections; i++) {
        /* W^X, the kernel should have been compiled in a way that this is valid. */
        int Flags = Sections[i].Characteristics & 0x20000000   ? PAGE_FLAGS_EXEC
                    : Sections[i].Characteristics & 0x80000000 ? PAGE_FLAGS_WRITE
                                                               : 0;

        uint32_t Size = Sections[i].VirtualSize;
        if (Sections[i].SizeOfRawData > Size) {
            Size = Sections[i].SizeOfRawData;
        }

        for (uint32_t Page = 0; Page < (Size + EFI_PAGE_SIZE - 1) >> EFI_PAGE_SHIFT; Page++) {
            ThisProgram->PageFlags[(Sections[i].VirtualAddress >> EFI_PAGE_SHIFT) + Page] = Flags;
        }

        if (Sections[i].SizeOfRawData) {
            memcpy(
                (char *)ThisProgram->PhysicalAddress + Sections[i].VirtualAddress,
                Buffer + Sections[i].PointerToRawData,
                Sections[i].SizeOfRawData);
        }

        if (Sections[i].VirtualSize > Sections[i].SizeOfRawData) {
            memset(
                (char *)ThisProgram->PhysicalAddress + Sections[i].VirtualAddress +
                    Sections[i].SizeOfRawData,
                0,
                Sections[i].VirtualSize - Sections[i].SizeOfRawData);
        }
    }

    /* Grab up this image's export table, we need it for the next boot step (after we load all
     * images). */
    if (Header->DataDirectories.ExportTable.Size) {
        PeExportHeader *ExportHeader =
            (PeExportHeader *)((char *)ThisProgram->PhysicalAddress +
                               Header->DataDirectories.ExportTable.VirtualAddress);
        uint32_t *AddressTable =
            (uint32_t *)((char *)ThisProgram->PhysicalAddress + ExportHeader->ExportTableRva);
        uint16_t *ExportOrdinals =
            (uint16_t *)((char *)ThisProgram->PhysicalAddress + ExportHeader->OrdinalTableRva);
        uint32_t *NamePointers =
            (uint32_t *)((char *)ThisProgram->PhysicalAddress + ExportHeader->NamePointerRva);

        ThisProgram->ExportTableSize = ExportHeader->NumberOfNamePointers;

        Status = gBS->AllocatePool(
            EfiLoaderData,
            ExportHeader->NumberOfNamePointers * sizeof(OslpExportEntry),
            (VOID **)&ThisProgram->ExportTable);
        if (Status != EFI_SUCCESS) {
            OslPrint("Failed to load a kernel/driver file.\r\n");
            OslPrint("The system ran out of memory while loading %s.\r\n", ImagePath);
            OslPrint("The boot process cannot continue.\r\n");
            return 0;
        }

        for (uint32_t i = 0; i < ExportHeader->NumberOfNamePointers; i++) {
            ThisProgram->ExportTable[i].Name =
                (char *)ThisProgram->PhysicalAddress + NamePointers[i];
            ThisProgram->ExportTable[i].Address =
                Header->ImageBase + AddressTable[ExportOrdinals[i]];
        }
    } else {
        ThisProgram->ExportTableSize = 0;
        ThisProgram->ExportTable = NULL;
    }

    /* We should have loaded the whole file, so it's safe to free the buffer. */
    gBS->FreePool(Buffer);
    RtAppendDList(LoadedPrograms, &ThisProgram->ListHeader);
    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function validates that all files imported by the loaded programs exist/are already
 *     loaded, and fixes up the import table.
 *
 * PARAMETERS:
 *     LoadedPrograms - Header of the loaded programs list.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int OslFixupImports(RtDList *LoadedPrograms) {
    for (RtDList *ListHeader = LoadedPrograms->Next; ListHeader != LoadedPrograms;
         ListHeader = ListHeader->Next) {
        OslpLoadedProgram *ThisProgram =
            CONTAINING_RECORD(ListHeader, OslpLoadedProgram, ListHeader);

        uint32_t Offset = *(uint32_t *)((char *)ThisProgram->PhysicalAddress + 0x3C);
        PeHeader *Header = (PeHeader *)((char *)ThisProgram->PhysicalAddress + Offset);

        if (Header->DataDirectories.ImportTable.Size) {
            PeImportHeader *ImportHeader =
                (PeImportHeader *)((char *)ThisProgram->PhysicalAddress +
                                   Header->DataDirectories.ImportTable.VirtualAddress);
            PeImportHeader *Limit =
                (PeImportHeader *)((char *)ImportHeader + Header->DataDirectories.ImportTable.Size);

            while (ImportHeader < Limit) {
                /* All zeroes is the `end of import directory` marker, we can assume we're done if
                   we reach it. */

                if (!ImportHeader->ImportLookupTableRva && !ImportHeader->TimeDateStamp &&
                    !ImportHeader->ForwarderChain && !ImportHeader->NameRva &&
                    !ImportHeader->ImportAddressTableRva) {
                    break;
                }

                int Found = 0;
                RtDList *OtherEntry = LoadedPrograms->Next;
                OslpLoadedProgram *ImportedProgram = NULL;

                while (OtherEntry != LoadedPrograms) {
                    ImportedProgram = CONTAINING_RECORD(OtherEntry, OslpLoadedProgram, ListHeader);

                    if (!strcmp(
                            (char *)ThisProgram->PhysicalAddress + ImportHeader->NameRva,
                            ImportedProgram->Name)) {
                        Found = 1;
                        break;
                    }

                    OtherEntry = OtherEntry->Next;
                }

                if (!Found) {
                    OslPrint("Failed to load a kernel/driver file.\r\n");
                    OslPrint(
                        "The kernel/driver %s tried importing from the non-existant file %s.\r\n",
                        ThisProgram->Name,
                        (char *)ThisProgram->PhysicalAddress + ImportHeader->NameRva);
                    OslPrint("The boot process cannot continue.\r\n");
                    return 0;
                }

                uint64_t *ImportTable = (uint64_t *)((char *)ThisProgram->PhysicalAddress +
                                                     ImportHeader->ImportLookupTableRva);
                uint64_t *AddressTable = (uint64_t *)((char *)ThisProgram->PhysicalAddress +
                                                      ImportHeader->ImportAddressTableRva);

                while (*ImportTable) {
                    /* We currently don't support import by ordinal; It should be easy enough to add
                       support for it later (TODO). */
                    if (*ImportTable & 0x8000000000000000) {
                        OslPrint("Failed to load a kernel/driver file.\r\n");
                        OslPrint(
                            "The kernel/driver %s tried importing by ordinal.\r\n",
                            ThisProgram->Name);
                        OslPrint("The boot process cannot continue.\r\n");
                        return 0;
                    }

                    char *SearchName = (char *)ThisProgram->PhysicalAddress + *(ImportTable++) + 2;
                    int Found = 0;

                    for (size_t i = 0; i < ImportedProgram->ExportTableSize; i++) {
                        if (!strcmp(ImportedProgram->ExportTable[i].Name, SearchName)) {
                            *(AddressTable++) = ImportedProgram->ExportTable[i].Address;
                            Found = 1;
                            break;
                        }
                    }

                    if (!Found) {
                        OslPrint("Failed to load a kernel/driver file.\r\n");
                        OslPrint(
                            "The kernel/driver %s tried importing the non-existant symbol %s from "
                            "%s.\r\n",
                            ThisProgram->Name,
                            SearchName,
                            ImportedProgram->Name);
                        OslPrint("The boot process cannot continue.\r\n");
                        return 0;
                    }
                }

                ImportHeader++;
            }
        }
    }

    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function applies all relocations; This function should be called last (after
 *     LoadProgram+FixupImports).
 *
 * PARAMETERS:
 *     LoadedPrograms - Header of the loaded programs list.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void OslFixupRelocations(RtDList *LoadedPrograms) {
    for (RtDList *ListHeader = LoadedPrograms->Next; ListHeader != LoadedPrograms;
         ListHeader = ListHeader->Next) {
        OslpLoadedProgram *ThisProgram =
            CONTAINING_RECORD(ListHeader, OslpLoadedProgram, ListHeader);

        uint32_t Offset = *(uint32_t *)((char *)ThisProgram->PhysicalAddress + 0x3C);
        PeHeader *Header = (PeHeader *)((char *)ThisProgram->PhysicalAddress + Offset);

        /* The relocation table is optional (it can be zero-sized if the executable is literally
         * empty/contains no code); But on most cases, we should have to do something. */
        if (!Header->DataDirectories.BaseRelocationTable.Size) {
            continue;
        }

        uint32_t Size = Header->DataDirectories.BaseRelocationTable.Size;
        char *Relocations = (char *)ThisProgram->PhysicalAddress +
                            Header->DataDirectories.BaseRelocationTable.VirtualAddress;

        while (Size) {
            PeBaseRelocationBlock *BaseRelocationBlock = (PeBaseRelocationBlock *)Relocations;
            char *BaseAddress = (char *)ThisProgram->PhysicalAddress + BaseRelocationBlock->PageRva;
            uint16_t *BlockRelocations = (uint16_t *)(BaseRelocationBlock + 1);
            uint32_t Entries =
                (BaseRelocationBlock->BlockSize - sizeof(PeBaseRelocationBlock)) >> 1;

            for (; Entries; Entries--) {
                uint16_t Type = *BlockRelocations >> 12;
                void *Offset = BaseAddress + (*(BlockRelocations++) & 0xFFF);

                switch (Type) {
                    case IMAGE_REL_BASED_HIGH:
                        *((uint16_t *)Offset) += ThisProgram->BaseDiff >> 16;
                        break;
                    case IMAGE_REL_BASED_LOW:
                        *((uint16_t *)Offset) += ThisProgram->BaseDiff;
                        break;
                    case IMAGE_REL_BASED_HIGHLOW:
                        *((uint32_t *)Offset) += ThisProgram->BaseDiff;
                        break;
                    case IMAGE_REL_BASED_HIGHADJ:
                        *((uint16_t *)Offset) += ThisProgram->BaseDiff >> 16;
                        *((uint16_t *)(BaseAddress + (*(BlockRelocations++) & 0xFFF))) =
                            ThisProgram->BaseDiff;
                        break;
                    case IMAGE_REL_BASED_DIR64:
                        *((uint64_t *)Offset) += ThisProgram->BaseDiff;
                        break;
                }
            }

            Size -= BaseRelocationBlock->BlockSize;
            Relocations += BaseRelocationBlock->BlockSize;
        }
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function mounts the main kernel module list, using the OSLOADER program list.
 *
 * PARAMETERS:
 *     LoadedPrograms - Header of the loaded programs list.
 *
 * RETURN VALUE:
 *     Head of the module list, or NULL if we failed to allocate anything.
 *-----------------------------------------------------------------------------------------------*/
RtDList *OslCreateKernelModuleList(RtDList *LoadedPrograms) {
    RtDList *ModuleListHead = NULL;
    EFI_STATUS Status = gBS->AllocatePool(EfiLoaderData, sizeof(RtDList), (VOID **)&ModuleListHead);
    if (Status != EFI_SUCCESS) {
        OslPrint("Failed to create the kernel module list.\r\n");
        OslPrint("The system ran out of memory while allocating the list head.\r\n");
        OslPrint("The boot process cannot continue.\r\n");
        return NULL;
    }

    RtInitializeDList(ModuleListHead);

    for (RtDList *ListHeader = LoadedPrograms->Next; ListHeader != LoadedPrograms;
         ListHeader = ListHeader->Next) {
        OslpModuleEntry *TargetEntry = NULL;
        Status = gBS->AllocatePool(EfiLoaderData, sizeof(OslpModuleEntry), (VOID **)&TargetEntry);
        if (Status != EFI_SUCCESS) {
            OslPrint("Failed to create the kernel module list.\r\n");
            OslPrint("The system ran out of memory while allocating a list item.\r\n");
            OslPrint("The boot process cannot continue.\r\n");
            return NULL;
        }

        OslpLoadedProgram *SourceEntry =
            CONTAINING_RECORD(ListHeader, OslpLoadedProgram, ListHeader);
        TargetEntry->ImageBase = SourceEntry->VirtualAddress;
        TargetEntry->EntryPoint = SourceEntry->EntryPoint;
        TargetEntry->SizeOfImage = SourceEntry->ImageSize;
        TargetEntry->ImageName = SourceEntry->Name;
        RtAppendDList(ModuleListHead, &TargetEntry->ListHeader);
    };

    return ModuleListHead;
}
