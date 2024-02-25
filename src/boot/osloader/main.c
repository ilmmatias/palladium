/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <acpi.h>
#include <console.h>
#include <file.h>
#include <graphics.h>
#include <loader.h>
#include <memory.h>
#include <platform.h>
#include <string.h>

EFI_HANDLE *gIH = NULL;
EFI_SYSTEM_TABLE *gST = NULL;
EFI_BOOT_SERVICES *gBS = NULL;
EFI_RUNTIME_SERVICES *gRT = NULL;

OslpBootBlock BootBlock __attribute__((aligned(DEFAULT_PAGE_ALLOCATION_GRANULARITY)));
RtDList OslpModuleListHead;

extern RtDList OslpMemoryDescriptorListHead;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function is the OSLOADER architecture-independent entry point.
 *     We detect and initialize all required hardware, load up the OS, and transfer control to it.
 *
 * PARAMETERS:
 *     ImageHandle - Data about the current executing image (osloader.efi).
 *     SystemTable - Pointer to the EFI system table.
 *
 * RETURN VALUE:
 *     Does not return on success.
 *-----------------------------------------------------------------------------------------------*/
EFI_STATUS
OslMain(EFI_HANDLE *ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    /* Save up any required EFI variables (so that we don't need to pass ImageHandle and
     * SystemTable around). */
    gIH = ImageHandle;
    gST = SystemTable;
    gBS = SystemTable->BootServices;
    gRT = SystemTable->RuntimeServices;

    /* Get rid of the watchdog timer (just to be sure). */
    gBS->SetWatchdogTimer(0, 0, 0, NULL);

    /* Initialize all required subsystems. */
    EFI_STATUS Status = OslpInitializeMemoryMap();
    if (Status != EFI_SUCCESS) {
        return Status;
    }

    Status = OslpInitializeRootVolume();
    if (Status != EFI_SUCCESS) {
        return Status;
    }

    void *AcpiTable = NULL;
    uint32_t AcpiTableVersion = 0;
    if (!OslpInitializeAcpi(&AcpiTable, &AcpiTableVersion)) {
        return EFI_LOAD_ERROR;
    }

    OslpInitializeEntropy();
    OslpInitializeVirtualAllocator();

    /* Let's get the actual boot process started; Load up KERNEL.EXE plus all boot drivers. */
    RtDList LoadedPrograms;
    RtInitializeDList(&LoadedPrograms);

    /* TODO: Use the UEFI command like to get the path to the kernel+to each driver, falling
     * back to this default on live/installer boot (with no arguments). */
    if (!OslLoadExecutable(&LoadedPrograms, "kernel.exe", "\\EFI\\PALLADIUM\\KERNEL.EXE") ||
        !OslLoadExecutable(&LoadedPrograms, "acpi.sys", "\\EFI\\PALLADIUM\\ACPI.SYS") ||
        !OslLoadExecutable(&LoadedPrograms, "pci.sys", "\\EFI\\PALLADIUM\\PCI.SYS")) {
        return EFI_LOAD_ERROR;
    }

    if (!OslFixupImports(&LoadedPrograms)) {
        return EFI_LOAD_ERROR;
    }

    OslFixupRelocations(&LoadedPrograms);

    /* Create the target/kernel module entry list; This is one of the structs the kernel will need
     * to relocate later (before marking the OSLOADER memory as free). */
    RtInitializeDList(&OslpModuleListHead);
    OslCreateKernelModuleList(&LoadedPrograms, &OslpModuleListHead);

    /* The boot processor structure is mostly architecture-dependent, so let's leave that to
     * ${ARCH}/bsp.c. */
    void *BootProcessor = OslpInitializeBsp();
    if (!BootProcessor) {
        return EFI_LOAD_ERROR;
    }

    /* Wrap up arch-specific preparations (this is the last place allocations are allowed!) */
    if (!OslpPrepareExecution(&LoadedPrograms, &OslpMemoryDescriptorListHead)) {
        return EFI_LOAD_ERROR;
    }

    /* Wrap up by setting up the graphics more, then, we should be ready to fill the boot block,
     * and transfer execution. */
    void *Framebuffer = NULL;
    uint32_t FramebufferWidth = 0;
    uint32_t FramebufferHeight = 0;
    uint32_t FramebufferPitch = 0;
    Status = OslpInitializeGraphics(
        &Framebuffer, &FramebufferWidth, &FramebufferHeight, &FramebufferPitch);
    if (Status != EFI_SUCCESS) {
        return EFI_LOAD_ERROR;
    }

    memcpy(BootBlock.Magic, OSLP_BOOT_MAGIC, 4);
    BootBlock.LoaderVersion = OSLP_BOOT_VERSION;
    BootBlock.MemoryDescriptorListHead = &OslpMemoryDescriptorListHead;
    BootBlock.BootDriverListHead = &OslpModuleListHead;
    BootBlock.BootProcessor = BootProcessor;
    BootBlock.AcpiTable = AcpiTable;
    BootBlock.AcpiTableVersion = AcpiTableVersion;
    BootBlock.Framebuffer = Framebuffer;
    BootBlock.FramebufferWidth = FramebufferWidth;
    BootBlock.FramebufferHeight = FramebufferHeight;
    BootBlock.FramebufferPitch = FramebufferPitch;
    OslpTransferExecution(&BootBlock);
}
