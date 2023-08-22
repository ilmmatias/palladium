/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

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
 *
 * RETURN VALUE:
 *     Physical address of the image, or 0 if we failed to load it.
 *-----------------------------------------------------------------------------------------------*/
static uint64_t LoadFile(const char *Path, uint64_t *VirtualAddress) {
    FILE *Stream = fopen(Path, "rb");
    if (!Stream) {
        return 0;
    }

    /* The PE data is prefixed with a MZ header and a MS-DOS stub; The offset into the PE data is
       guaranteed to be after the main MZ header. */
    uint32_t Offset;
    if (fseek(Stream, 0x3C, SEEK_SET) || fread(&Offset, sizeof(uint32_t), 1, Stream) != 1) {
        fclose(Stream);
        return 0;
    }

    PeHeader InitialHeader;
    PeHeader *Header = &InitialHeader;
    if (fseek(Stream, Offset, SEEK_SET) || fread(Header, sizeof(PeHeader), 1, Stream) != 1) {
        fclose(Stream);
        return 0;
    }

    /* Following the information at https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
       for the implementation. */
    if (memcmp(Header->Signature, PE_SIGNATURE, 4) || Header->Machine != PE_MACHINE ||
        Header->Magic != PE_MAGIC) {
        fclose(Stream);
        return 0;
    }

    uint64_t LargePages = (Header->SizeOfImage + LARGE_PAGE_SIZE - 1) >> LARGE_PAGE_SHIFT;
    void *PhysicalAddress = malloc(LargePages << LARGE_PAGE_SHIFT);
    if (!PhysicalAddress) {
        fclose(Stream);
        return 0;
    }

    *VirtualAddress = BmAllocateVirtualAddress(LargePages);
    if (!*VirtualAddress) {
        free(PhysicalAddress);
        fclose(Stream);
        return 0;
    }

    /* The kernel might use information from the base headers and section headers; SizeOfImage
       should have given us the code/data size + all headers, so we're assuming that and loading
       it all up to the base address. */
    if (fseek(Stream, 0, SEEK_SET) ||
        fread(PhysicalAddress, Header->SizeOfHeaders, 1, Stream) != 1) {
        free(PhysicalAddress);
        fclose(Stream);
        return 0;
    }

    uint64_t ExpectedBase = Header->ImageBase;
    uint64_t BaseDiff = *VirtualAddress - ExpectedBase;
    Header = PhysicalAddress;
    Header->ImageBase = *VirtualAddress;

    /* We can be almost certain that high half > than whatever the linker put us at, but do
       handle a possible underflow anyways. */
    if (*VirtualAddress < ExpectedBase) {
        BaseDiff = ExpectedBase - *VirtualAddress;
    }

    (void)BaseDiff;
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
void BmLoadPalladium(const char *SystemFolder) {
    BmSetColor(DISPLAY_COLOR_DEFAULT);
    BmInitDisplay();

    do {
        size_t KernelPathSize = strlen(SystemFolder) + 12;
        char *KernelPath = malloc(KernelPathSize);
        if (!KernelPath) {
            break;
        }

        snprintf(KernelPath, KernelPathSize, "%s/kernel.exe", SystemFolder);

        /* Delegate the loading job to the common PE loader. */
        uint64_t KernelVirtualAddress;
        uint64_t KernelPhysicalAddress = LoadFile(KernelPath, &KernelVirtualAddress);
        if (!KernelPhysicalAddress) {
            free(KernelPath);
            break;
        }
    } while (0);

    BmPollKey();
}
