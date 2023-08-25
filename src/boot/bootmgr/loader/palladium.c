/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <boot.h>
#include <display.h>
#include <keyboard.h>
#include <memory.h>
#include <pe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
 *
 * RETURN VALUE:
 *     Physical address of the image, or 0 if we failed to load it.
 *-----------------------------------------------------------------------------------------------*/
static uint64_t LoadFile(
    const char *Path,
    uint64_t *VirtualAddress,
    uint64_t *EntryAddress,
    uint64_t *ImageSize,
    int **PageFlags) {
    FILE *Stream = fopen(Path, "rb");
    if (!Stream) {
        return 0;
    }

    /* The PE data is prefixed with a MZ header and a MS-DOS stub; The offset into the PE data is
       guaranteed to be after the main MZ header. */
    uint32_t Offset;
    if (fseek(Stream, 0x3C, SEEK_SET) || fread(&Offset, sizeof(uint32_t), 1, Stream) != 1) {
        return 0;
    }

    PeHeader InitialHeader;
    PeHeader *Header = &InitialHeader;
    if (fseek(Stream, Offset, SEEK_SET) || fread(Header, sizeof(PeHeader), 1, Stream) != 1) {
        return 0;
    }

    /* Following the information at https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
       for the implementation. */
    if (memcmp(Header->Signature, PE_SIGNATURE, 4) || Header->Machine != PE_MACHINE ||
        (Header->Characteristics & 0x2000) || Header->Magic != 0x20B || Header->Subsystem != 1 ||
        (Header->DllCharacteristics & 0x160) != 0x160) {
        return 0;
    }

    uint64_t Pages = (Header->SizeOfImage + PAGE_SIZE - 1) >> PAGE_SHIFT;
    *ImageSize = Pages << PAGE_SHIFT;

    *PageFlags = calloc(Pages, sizeof(int));
    if (!*PageFlags) {
        return 0;
    }

    void *PhysicalAddress = BmAllocatePages(*ImageSize >> PAGE_SHIFT, MEMORY_KERNEL);
    if (!PhysicalAddress) {
        return 0;
    }

    *VirtualAddress = BmAllocateVirtualAddress(Pages);
    if (!*VirtualAddress) {
        return 0;
    }

    /* The kernel might use information from the base headers and section headers; SizeOfImage
       should have given us the code/data size + all headers, so we're assuming that and loading
       it all up to the base address. */
    if (fseek(Stream, 0, SEEK_SET) ||
        fread(PhysicalAddress, Header->SizeOfHeaders, 1, Stream) != 1) {
        return 0;
    }

    uint64_t ExpectedBase = Header->ImageBase;
    uint64_t BaseDiff = *VirtualAddress - ExpectedBase;
    Header = (PeHeader *)((char *)PhysicalAddress + Offset);
    Header->ImageBase = *VirtualAddress;
    *EntryAddress = *VirtualAddress + Header->AddressOfEntryPoint;

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

        if (fseek(Stream, Sections[i].PointerToRawData, SEEK_SET) ||
            fread(
                (char *)PhysicalAddress + Sections[i].VirtualAddress,
                Sections[i].SizeOfRawData,
                1,
                Stream) != 1) {
            return 0;
        }

        if (Sections[i].VirtualSize > Sections[i].SizeOfRawData) {
            memset(
                (char *)PhysicalAddress + Sections[i].VirtualAddress + Sections[i].SizeOfRawData,
                0,
                Sections[i].VirtualSize - Sections[i].SizeOfRawData);
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
[[noreturn]] void BmLoadPalladium(const char *SystemFolder) {
    BmSetColor(DISPLAY_COLOR_DEFAULT);
    BmInitDisplay();

    do {
        /* This should BmPanic() on incompatible machines, no need for a return code. */
        BmCheckCompatibility();

        size_t KernelPathSize = strlen(SystemFolder) + 12;
        char *KernelPath = malloc(KernelPathSize);
        if (!KernelPath) {
            break;
        }

        snprintf(KernelPath, KernelPathSize, "%s/kernel.exe", SystemFolder);

        /* Delegate the loading job to the common PE loader. */
        uint64_t VirtualAddress;
        uint64_t EntryPoint;
        uint64_t ImageSize;
        int *PageFlags;
        uint64_t PhysicalAddress =
            LoadFile(KernelPath, &VirtualAddress, &EntryPoint, &ImageSize, &PageFlags);
        if (!PhysicalAddress) {
            break;
        }

        BmTransferExecution(VirtualAddress, PhysicalAddress, ImageSize, EntryPoint, PageFlags);
    } while (0);

    BmPanic("An error occoured while trying to load the selected operating system.\n"
            "Please, reboot your device and try again.\n");
}
