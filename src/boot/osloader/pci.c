/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <pci.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function searches for any PCI devices matching the specified class code.
 *
 * PARAMETERS:
 *     ClassId - Which class code the device should have.
 *     Handles - Output; Where to store the handle list.
 *
 * RETURN VALUE:
 *     Size of the handle list.
 *-----------------------------------------------------------------------------------------------*/
UINTN OslFindPciDevicesByClass(uint16_t ClassId, EFI_PCI_IO_PROTOCOL ***Devices) {
    /* EFI firmwares nicely lay out all valid PCI devices as PCI_IO_PROTOCOL handles, so we don't
     * need to manually probe for all possible valid devices.  */
    UINTN HandleCount = 0;
    EFI_HANDLE *Handles = NULL;
    EFI_STATUS Status =
        gBS->LocateHandleBuffer(ByProtocol, &gEfiPciIoProtocolGuid, NULL, &HandleCount, &Handles);
    if (Status != EFI_SUCCESS) {
        return 0;
    }

    Status = gBS->AllocatePool(
        EfiLoaderData, sizeof(EFI_PCI_IO_PROTOCOL *) * HandleCount, (VOID **)Devices);
    if (Status != EFI_SUCCESS) {
        return 0;
    }

    UINTN DeviceCount = 0;
    for (UINTN i = 0; i < HandleCount; i++) {
        EFI_PCI_IO_PROTOCOL *PciIo = NULL;
        Status = gBS->HandleProtocol(Handles[i], &gEfiPciIoProtocolGuid, (VOID **)&PciIo);
        if (Status != EFI_SUCCESS) {
            continue;
        }

        PCI_TYPE00 Header;
        Status = PciIo->Pci.Read(
            PciIo, EfiPciIoWidthUint32, 0, sizeof(PCI_TYPE00) / sizeof(UINT32), &Header);
        if (Status == EFI_SUCCESS && Header.Hdr.ClassCode[2] == ClassId) {
            (*Devices)[DeviceCount++] = PciIo;
        }
    }

    return DeviceCount;
}
