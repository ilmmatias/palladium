/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <console.h>
#include <memory.h>
#include <string.h>

static EFI_FILE_HANDLE OslpRootVolume = NULL;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function opens the boot/root volume, getting the system ready to open the kernel (and
 *     driver) files.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
EFI_STATUS OslpInitializeRootVolume(void) {
    EFI_LOADED_IMAGE *LoadedImage = NULL;
    EFI_FILE_IO_INTERFACE *VolumeIo = NULL;
    EFI_STATUS Status;

    do {
        Status = gBS->HandleProtocol(gIH, &gEfiLoadedImageProtocolGuid, (VOID **)&LoadedImage);
        if (Status != EFI_SUCCESS) {
            break;
        }

        Status = gBS->HandleProtocol(
            LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID **)&VolumeIo);
        if (Status != EFI_SUCCESS) {
            break;
        }

        Status = VolumeIo->OpenVolume(VolumeIo, &OslpRootVolume);
    } while (0);

    if (Status != EFI_SUCCESS) {
        OslPrint("Failed to open the root/boot partition.\r\n");
        OslPrint("There might be something wrong with your UEFI firmware.\r\n");
        OslPrint("The boot process cannot continue.\r\n");
    }

    return Status;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function opens a file relative to the boot volume root, and reads all its contents.
 *
 * PARAMETERS:
 *     Path - Path relative to the root; This needs to be in the UEFI format (using
 *            backslashes/Windows-like).
 *     Size - Size of the buffer; Use this do OslFreePages() if required.
 *
 * RETURN VALUE:
 *     Either a buffer allocated using OslAllocatePages containing all the file data, or NULL.
 *-----------------------------------------------------------------------------------------------*/
void *OslReadFile(const char *Path, uint64_t *Size) {
    size_t PathSize = strlen(Path);
    CHAR16 Path16[strlen(Path) + 1];
    Path16[PathSize] = 0;
    for (size_t i = 0; i < PathSize; i++) {
        Path16[i] = Path[i];
    }

    EFI_FILE_HANDLE Handle = NULL;
    EFI_STATUS Status = OslpRootVolume->Open(
        OslpRootVolume,
        &Handle,
        Path16,
        EFI_FILE_MODE_READ,
        EFI_FILE_READ_ONLY | EFI_FILE_HIDDEN | EFI_FILE_SYSTEM);
    if (Status != EFI_SUCCESS) {
        return NULL;
    }

    /* We need to dynamically allocate the FileInfo buffer. */
    UINTN FileInfoSize = 0;
    EFI_FILE_INFO *FileInfo = NULL;

    while (1) {
        UINTN PreviousBufferSize = FileInfoSize;
        Status = Handle->GetInfo(Handle, &gEfiFileInfoGuid, &FileInfoSize, (VOID *)FileInfo);
        if (Status != EFI_BUFFER_TOO_SMALL) {
            break;
        }

        if (FileInfo) {
            OslFreePages(FileInfo, PreviousBufferSize);
        }

        FileInfo = OslAllocatePages(FileInfoSize, PAGE_OSLOADER);
        if (!FileInfo) {
            Status = EFI_OUT_OF_RESOURCES;
            break;
        }
    }

    if (Status != EFI_SUCCESS) {
        Handle->Close(Handle);
        return NULL;
    }

    /* Now we just need to allocate enough space for the destination buffer, and we should be
     * ready to go. */
    *Size = FileInfo->FileSize;
    OslFreePages(FileInfo, FileInfoSize);

    void *Buffer = OslAllocatePages(*Size, PAGE_OSLOADER);
    if (!Buffer) {
        Handle->Close(Handle);
        return NULL;
    }

    Status = Handle->Read(Handle, Size, Buffer);
    if (Status != EFI_SUCCESS) {
        OslFreePages(Buffer, *Size);
        Handle->Close(Handle);
        return NULL;
    }

    Handle->Close(Handle);
    return Buffer;
}
