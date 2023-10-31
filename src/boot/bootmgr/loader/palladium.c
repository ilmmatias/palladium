/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <boot.h>
#include <display.h>
#include <keyboard.h>
#include <memory.h>
#include <pe.h>
#include <registry.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *Name;
    uint64_t Address;
} ExportEntry;

typedef struct {
    const char *Name;
    size_t EntryCount;
    ExportEntry *Entries;
} ExportTable;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function loads up the specified PE file, validating the target architecture, and if
 *     it's an EXE (kernel) or SYS (driver) file.
 *     KASLR is enabled at all times, with the high bits of the virtual address being randomized.
 *
 * PARAMETERS:
 *     Path - Full path of the file.
 *     VirtualAddress - Output; Chosen virual address for the image.
 *     EntryAddress - Output; Where we should jump to reach the image entry point.
 *     ImageSize - Output; Size in bytes of the loaded image.
 *     IsKernel - Specifies if this is the main kernel image.
 *     CurrentImageCount - How many images have been loaded before us.
 *     OtherImages - Start of the image array.
 *     ThisImage - Pointer into where we should save this image's export table.
 *
 * RETURN VALUE:
 *     Physical address of the image, or 0 if we failed to load it.
 *-----------------------------------------------------------------------------------------------*/
static uint64_t LoadFile(
    const char *Path,
    uint64_t *VirtualAddress,
    uint64_t *EntryAddress,
    uint64_t *ImageSize,
    int **PageFlags,
    int IsKernel,
    size_t CurrentImageCount,
    ExportTable *OtherImages,
    ExportTable *ThisImage) {
    BmPrint("loading up %s\n", Path);

    FileContext *Stream = BmOpenFile(Path);
    if (!Stream) {
        return 0;
    }

    /* The PE data is prefixed with a MZ header and a MS-DOS stub; The offset into the PE data is
       guaranteed to be after the main MZ header. */
    uint32_t Offset;
    if (BmReadFile(Stream, &Offset, 0x3C, sizeof(uint32_t), NULL)) {
        BmCloseFile(Stream);
        return 0;
    }

    PeHeader InitialHeader;
    PeHeader *Header = &InitialHeader;
    if (BmReadFile(Stream, Header, Offset, sizeof(PeHeader), NULL)) {
        BmCloseFile(Stream);
        return 0;
    }

    /* Following the information at https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
       for the implementation. */
    if (memcmp(Header->Signature, PE_SIGNATURE, 4) || Header->Machine != PE_MACHINE ||
        (Header->Characteristics & 0x2000) || Header->Magic != 0x20B || Header->Subsystem != 1 ||
        (Header->DllCharacteristics & 0x160) != 0x160) {
        BmCloseFile(Stream);
        return 0;
    }

    /* Change the following parameter if we ever need to increase the stack size!
       Our current default is 16KiB. */
    const uint64_t StackSize = 0x4000;

    uint64_t FirstStackPage = (Header->SizeOfImage + PAGE_SIZE - 1) >> PAGE_SHIFT;
    uint64_t StackPageCount = IsKernel ? (StackSize + PAGE_SIZE - 1) >> PAGE_SHIFT : 0;

    uint64_t Pages = FirstStackPage + StackPageCount;
    *ImageSize = Pages << PAGE_SHIFT;

    *PageFlags = BmAllocateZeroBlock(Pages, sizeof(int));
    if (!*PageFlags) {
        BmCloseFile(Stream);
        return 0;
    }

    void *PhysicalAddress = BmAllocatePages(Pages, MEMORY_KERNEL);
    if (!PhysicalAddress) {
        BmCloseFile(Stream);
        return 0;
    }

    *VirtualAddress = BmAllocateVirtualAddress(Pages);
    if (!*VirtualAddress) {
        BmCloseFile(Stream);
        return 0;
    }

    /* We already know which flags we want for the stack pages (RW-only, no exec). */
    for (uint64_t i = 0; i < StackPageCount; i++) {
        (*PageFlags)[FirstStackPage + i] = PAGE_WRITE;
    }

    /* The kernel might use information from the base headers and section headers; SizeOfImage
       should have given us the code/data size + all headers, so we're assuming that and loading
       it all up to the base address. */
    if (BmReadFile(Stream, PhysicalAddress, 0, Header->SizeOfHeaders, NULL)) {
        BmCloseFile(Stream);
        return 0;
    }

    uint64_t ExpectedBase = Header->ImageBase;
    uint64_t BaseDiff = *VirtualAddress - ExpectedBase;
    Header = (PeHeader *)((char *)PhysicalAddress + Offset);
    Header->ImageBase = *VirtualAddress;

    if (EntryAddress) {
        *EntryAddress = *VirtualAddress + Header->AddressOfEntryPoint;
    }

    PeSectionHeader *Sections =
        (PeSectionHeader *)((char *)PhysicalAddress + Offset + Header->SizeOfOptionalHeader + 24);

    for (uint16_t i = 0; i < Header->NumberOfSections; i++) {
        /* W^X, the kernel should have been compiled in a way that this is valid. */
        int Flags = Sections[i].Characteristics & 0x20000000   ? PAGE_EXEC
                    : Sections[i].Characteristics & 0x80000000 ? PAGE_WRITE
                                                               : 0;

        uint32_t Size = Sections[i].VirtualSize;
        if (Sections[i].SizeOfRawData > Size) {
            Size = Sections[i].SizeOfRawData;
        }

        for (uint32_t Page = 0; Page < ((Size + PAGE_SIZE - 1) >> PAGE_SHIFT); Page++) {
            (*PageFlags)[(Sections[i].VirtualAddress >> PAGE_SHIFT) + Page] = Flags;
        }

        if (Sections[i].SizeOfRawData) {
            if (BmReadFile(
                    Stream,
                    (char *)PhysicalAddress + Sections[i].VirtualAddress,
                    Sections[i].PointerToRawData,
                    Sections[i].SizeOfRawData,
                    NULL)) {
                BmCloseFile(Stream);
                return 0;
            }
        }

        if (Sections[i].VirtualSize > Sections[i].SizeOfRawData) {
            memset(
                (char *)PhysicalAddress + Sections[i].VirtualAddress + Sections[i].SizeOfRawData,
                0,
                Sections[i].VirtualSize - Sections[i].SizeOfRawData);
        }
    }

    /* We should have loaded the whole file, so it's safe to close the handle. */
    BmCloseFile(Stream);

    /* Grab up this image's export table, and its name; We need to parse the path to get the
       image's name (get everything after the last slash). */

    ThisImage->Name = strrchr(Path, '/');
    if (!(ThisImage->Name++)) {
        /* All paths should be absolute (and they will ALWAYS contain at least one slash),
           something's broken if that's not the case. */
        return 0;
    }

    if (Header->DataDirectories.ExportTable.Size) {
        PeExportHeader *ExportHeader =
            (PeExportHeader *)((char *)PhysicalAddress +
                               Header->DataDirectories.ExportTable.VirtualAddress);
        uint32_t *AddressTable =
            (uint32_t *)((char *)PhysicalAddress + ExportHeader->ExportTableRva);
        uint16_t *ExportOrdinals =
            (uint16_t *)((char *)PhysicalAddress + ExportHeader->OrdinalTableRva);
        uint32_t *NamePointers =
            (uint32_t *)((char *)PhysicalAddress + ExportHeader->NamePointerRva);

        ThisImage->EntryCount = ExportHeader->NumberOfNamePointers;
        ThisImage->Entries =
            BmAllocateZeroBlock(ExportHeader->NumberOfNamePointers, sizeof(ExportEntry));

        for (uint32_t i = 0; i < ExportHeader->NumberOfNamePointers; i++) {
            ThisImage->Entries[i].Name = (char *)PhysicalAddress + NamePointers[i];
            ThisImage->Entries[i].Address = Header->ImageBase + AddressTable[ExportOrdinals[i]];
        }
    } else {
        ThisImage->EntryCount = 0;
        ThisImage->Entries = NULL;
    }

    /* Importing is invalid in the kernel; Drivers are allowed to import from any loaded image
       but themselves. */
    if (IsKernel && Header->DataDirectories.ImportTable.Size) {
        return 0;
    } else if (Header->DataDirectories.ImportTable.Size) {
        PeImportHeader *ImportHeader =
            (PeImportHeader *)((char *)PhysicalAddress +
                               Header->DataDirectories.ImportTable.VirtualAddress);
        PeImportHeader *Limit =
            (PeImportHeader *)((char *)ImportHeader + Header->DataDirectories.ImportTable.Size);

        while (ImportHeader < Limit) {
            /* All zeroes is the `end of import directory` marker, we can assume we're done if we
               reach it. */

            if (!ImportHeader->ImportLookupTableRva && !ImportHeader->TimeDateStamp &&
                !ImportHeader->ForwarderChain && !ImportHeader->NameRva &&
                !ImportHeader->ImportAddressTableRva) {
                break;
            }

            int Found = 0;
            ExportTable *ImageExports = OtherImages;
            for (size_t i = 0; i < CurrentImageCount; i++) {
                if (!strcmp((char *)PhysicalAddress + ImportHeader->NameRva, ImageExports->Name)) {
                    Found = 1;
                    break;
                }

                ImageExports++;
            }

            if (!Found) {
                return 0;
            }

            uint64_t *ImportTable =
                (uint64_t *)((char *)PhysicalAddress + ImportHeader->ImportLookupTableRva);
            uint64_t *AddressTable =
                (uint64_t *)((char *)PhysicalAddress + ImportHeader->ImportAddressTableRva);

            while (*ImportTable) {
                /* We currently don't support import by ordinal; It should be easy enough to add
                   support for it later (TODO). */
                if (*ImportTable & 0x8000000000000000) {
                    return 0;
                }

                char *SearchName = (char *)PhysicalAddress + *(ImportTable++) + 2;
                int Found = 0;

                for (size_t i = 0; i < ImageExports->EntryCount; i++) {
                    if (!strcmp(ImageExports->Entries[i].Name, SearchName)) {
                        *(AddressTable++) = ImageExports->Entries[i].Address;
                        Found = 1;
                        break;
                    }
                }

                if (!Found) {
                    return 0;
                }
            }

            ImportHeader++;
        }
    }

    /* The relocation table is optional (it can be zero-sized if the executable is literally
       empty/contains no code); But on most cases, we should have to do something. */
    if (!Header->DataDirectories.BaseRelocationTable.Size) {
        return (uint64_t)PhysicalAddress;
    }

    uint32_t Size = Header->DataDirectories.BaseRelocationTable.Size;
    char *Relocations =
        (char *)PhysicalAddress + Header->DataDirectories.BaseRelocationTable.VirtualAddress;

    while (Size) {
        PeBaseRelocationBlock *BaseRelocationBlock = (PeBaseRelocationBlock *)Relocations;
        char *BaseAddress = (char *)PhysicalAddress + BaseRelocationBlock->PageRva;
        uint16_t *BlockRelocations = (uint16_t *)(BaseRelocationBlock + 1);
        uint32_t Entries = (BaseRelocationBlock->BlockSize - sizeof(PeBaseRelocationBlock)) >> 1;

        for (; Entries; Entries--) {
            uint16_t Type = *BlockRelocations >> 12;
            void *Offset = BaseAddress + (*(BlockRelocations++) & 0xFFF);

            switch (Type) {
                case IMAGE_REL_BASED_HIGH:
                    *((uint16_t *)Offset) += BaseDiff >> 16;
                    break;
                case IMAGE_REL_BASED_LOW:
                    *((uint16_t *)Offset) += BaseDiff;
                    break;
                case IMAGE_REL_BASED_HIGHLOW:
                    *((uint32_t *)Offset) += BaseDiff;
                    break;
                case IMAGE_REL_BASED_HIGHADJ:
                    *((uint16_t *)Offset) += BaseDiff >> 16;
                    *((uint16_t *)(BaseAddress + (*(BlockRelocations++) & 0xFFF))) = BaseDiff;
                    break;
                case IMAGE_REL_BASED_DIR64:
                    *((uint64_t *)Offset) += BaseDiff;
                    break;
            }
        }

        Size -= BaseRelocationBlock->BlockSize;
        Relocations += BaseRelocationBlock->BlockSize;
    }

    return (uint64_t)PhysicalAddress;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function loads up the system located in the given folder, jumping to execution
 *     afterwards.
 *
 * PARAMETERS:
 *     SystemFolder - Full path of the system folder (device()/path/to/System).
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void BiLoadPalladium(const char *SystemFolder) {
    const char *Message = "An error occoured while trying to load the selected operating system.\n"
                          "Please, reboot your device and try again.\n";

    BmSetColor(DISPLAY_COLOR_DEFAULT);
    BmResetDisplay();

    do {
        /* This should BmPanic() on incompatible machines, no need for a return code. */
        BiCheckCompatibility();

        size_t SystemFolderSize = strlen(SystemFolder);
        size_t KernelPathSize = SystemFolderSize + 12;
        char *KernelPath = BmAllocateBlock(KernelPathSize);
        char *RegistryPath = BmAllocateBlock(KernelPathSize);
        if (!KernelPath || !RegistryPath) {
            break;
        }

        snprintf(KernelPath, KernelPathSize, "%s/kernel.exe", SystemFolder);
        snprintf(RegistryPath, KernelPathSize, "%s/kernel.reg", SystemFolder);

        /* Load up the kernel registry, it should be located adjacent to the kernel image.
           We need it to find all the boot-time drivers (like important FS drivers). */
        RegHandle *Handle = BmLoadRegistry(RegistryPath);
        if (!Handle) {
            Message =
                "An error occoured while trying to load the selected operating system.\n"
                "The kernel registry file inside the System folder is invalid or corrupted.\n";
            break;
        }

        /* Failing this is a probable "inaccessible boot device" error later on the kernel
           initialization process, so crash early. */
        RegEntryHeader *DriverEntries = BmFindRegistryEntry(Handle, NULL, "Drivers");
        if (!DriverEntries) {
            Message =
                "An error occoured while trying to load the selected operating system.\n"
                "The kernel registry file inside the System folder is invalid or corrupted.\n";
            break;
        }

        /* First pass, collect the driver count, as we'll be allocating the LoadedImage array all
           at once. */
        int DriverCount = 0;
        for (int i = 0;; i++) {
            RegEntryHeader *Entry = BmGetRegistryEntry(Handle, DriverEntries, i);

            if (!Entry) {
                break;
            } else if (
                Entry->Type != REG_ENTRY_DWORD ||
                !*((uint32_t *)((char *)Entry + Entry->Length - 4))) {
                continue;
            }

            DriverCount++;
        }

        /* We're forced to use BmAllocatePages, as we need to mark this page as KERNEL instead of
           BOOT. */
        LoadedImage *Images = BmAllocatePages(
            ((DriverCount + 1) * sizeof(LoadedImage) + PAGE_SIZE - 1) >> PAGE_SHIFT, MEMORY_KERNEL);
        if (!Images) {
            break;
        }

        size_t ExportTableSize = 0;
        ExportTable *Exports = BmAllocateBlock((DriverCount + 1) * sizeof(ExportTable));
        if (!Exports) {
            break;
        }

        /* Delegate the loading job to the common PE loader. */
        Images[0].PhysicalAddress = LoadFile(
            KernelPath,
            &Images[0].VirtualAddress,
            &Images[0].EntryPoint,
            &Images[0].ImageSize,
            &Images[0].PageFlags,
            1,
            ExportTableSize++,
            Exports,
            Exports);

        if (!Images[0].PhysicalAddress) {
            Message = "An error occoured while trying to load the selected operating system.\n"
                      "The kernel file inside the System folder is invalid or corrupted.\n";
            break;
        }

        /* Second pass, load up all the drivers, solving any driver->kernel imports. */
        int Fail = 0;

        DriverCount = 0;

        for (int i = 0;; i++) {
            RegEntryHeader *Entry = BmGetRegistryEntry(Handle, DriverEntries, i);

            if (!Entry) {
                break;
            } else if (
                Entry->Type != REG_ENTRY_DWORD ||
                !*((uint32_t *)((char *)Entry + Entry->Length - 4))) {
                continue;
            }

            size_t DriverPathSize = SystemFolderSize + strlen((char *)(Entry + 1)) + 2;
            char *DriverPath = BmAllocateBlock(DriverPathSize);
            if (!DriverPath) {
                Fail = 1;
                break;
            }

            snprintf(DriverPath, DriverPathSize, "%s/%s", SystemFolder, Entry + 1);

            Images[DriverCount + 1].PhysicalAddress = LoadFile(
                DriverPath,
                &Images[DriverCount + 1].VirtualAddress,
                &Images[DriverCount + 1].EntryPoint,
                &Images[DriverCount + 1].ImageSize,
                &Images[DriverCount + 1].PageFlags,
                0,
                ExportTableSize++,
                Exports,
                Exports + i + 1);

            if (!Images[++DriverCount].PhysicalAddress) {
                Message = "An error occoured while trying to load the selected operating system.\n"
                          "One of the boot-time drivers is invalid or corrupted.\n";
                Fail = 1;
                break;
            }
        }

        if (Fail) {
            break;
        }

        BiTransferExecution(Images, DriverCount + 1);
    } while (0);

    BmPanic(Message);
}
