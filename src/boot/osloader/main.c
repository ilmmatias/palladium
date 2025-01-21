/* SPDX-FileCopyrightText: (C) 2024-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <acpi.h>
#include <console.h>
#include <entropy.h>
#include <file.h>
#include <graphics.h>
#include <loader.h>
#include <platform.h>
#include <string.h>

EFI_HANDLE *gIH = NULL;
EFI_SYSTEM_TABLE *gST = NULL;
EFI_BOOT_SERVICES *gBS = NULL;
EFI_RUNTIME_SERVICES *gRT = NULL;

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

    /* Get the RNG ready for randomizing the virtual memory load addresses. */
    OslpInitializeEntropy();

    /* Initialize all required subsystems. */
    EFI_STATUS Status = OslpInitializeRootVolume();
    if (Status != EFI_SUCCESS) {
        return EFI_LOAD_ERROR;
    }

    Status = OslpInitializeVirtualAllocator();
    if (Status != EFI_SUCCESS) {
        return EFI_LOAD_ERROR;
    }

    void *AcpiTable = NULL;
    uint32_t AcpiTableVersion = 0;
    if (!OslpInitializeAcpi(&AcpiTable, &AcpiTableVersion)) {
        return EFI_LOAD_ERROR;
    }

    void *BackBuffer = NULL;
    void *FrontBuffer = NULL;
    uint32_t FramebufferWidth = 0;
    uint32_t FramebufferHeight = 0;
    uint32_t FramebufferPitch = 0;
    Status = OslpInitializeGraphics(
        &BackBuffer, &FrontBuffer, &FramebufferWidth, &FramebufferHeight, &FramebufferPitch);
    if (Status != EFI_SUCCESS) {
        return EFI_LOAD_ERROR;
    }

    /* Let's get the actual boot process started; Load up KERNEL.EXE plus all boot drivers. */
    RtDList LoadedPrograms;
    RtInitializeDList(&LoadedPrograms);

    /* TODO: Load up the kernel+boot driver paths using another method (UEFI command line? Or maybe
     * a config file?). */
    if (!OslLoadExecutable(&LoadedPrograms, "kernel.exe", "\\EFI\\PALLADIUM\\KERNEL.EXE") ||
        !OslLoadExecutable(&LoadedPrograms, "acpi.sys", "\\EFI\\PALLADIUM\\ACPI.SYS")) {
        return EFI_LOAD_ERROR;
    }

    if (!OslFixupImports(&LoadedPrograms)) {
        return EFI_LOAD_ERROR;
    }

    OslFixupRelocations(&LoadedPrograms);

    /* Create the target/kernel module entry list (this is what the kernel will have access, as the
     * LoadedPrograms list is internal to us). */
    RtDList *ModuleListHead = OslCreateKernelModuleList(&LoadedPrograms);
    if (!ModuleListHead) {
        return EFI_LOAD_ERROR;
    }

    /* We need to create a small (8KiB) temporary stack (for use during the kernel BSP
     * initialization, as the current UEFI stack probably won't be mapped in). */
    void *BootStack = OslAllocatePages(SIZE_8KB, SIZE_4KB, PAGE_TYPE_OSLOADER_TEMPORARY);
    if (!BootStack) {
        OslPrint("Failed to allocate space for the boot stack.\r\n");
        OslPrint("The boot process cannot continue.\r\n");
        return EFI_LOAD_ERROR;
    }

    /* Let's try to build the memory descriptors now (we still have some allocations left
     * to do, but let's guess it's safe to do it now).*/
    RtDList *MemoryDescriptorListHead = NULL;
    RtDList MemoryDescriptorStack = {};
    UINTN MemoryMapSize = 0;
    EFI_MEMORY_DESCRIPTOR *MemoryMap = NULL;
    UINTN DescriptorSize = 0;
    UINT32 DescriptorVersion = 0;
    if (!OslpCreateMemoryDescriptors(
            &MemoryDescriptorListHead,
            &MemoryDescriptorStack,
            &MemoryMapSize,
            &MemoryMap,
            &DescriptorSize,
            &DescriptorVersion)) {
        return EFI_LOAD_ERROR;
    }

    /* We can now wrap up by filling the boot block, and creating the page map. */
    OslpBootBlock *BootBlock = NULL;
    Status = gBS->AllocatePool(EfiLoaderData, sizeof(OslpBootBlock), (VOID **)&BootBlock);
    if (Status != EFI_SUCCESS) {
        OslPrint("Failed to allocate space for the boot block.\r\n");
        OslPrint("The boot process cannot continue.\r\n");
        return EFI_LOAD_ERROR;
    }

    /* All important allocations from now on need to update the descriptor list manually! */
    if (!OslpUpdateMemoryDescriptors(
            MemoryDescriptorListHead,
            &MemoryDescriptorStack,
            PAGE_TYPE_OSLOADER_TEMPORARY,
            (uint64_t)BootBlock >> EFI_PAGE_SHIFT,
            (sizeof(OslpBootBlock) + EFI_PAGE_SIZE - 1) >> EFI_PAGE_SHIFT)) {
        return EFI_LOAD_ERROR;
    }

    memcpy(BootBlock->Magic, OSLP_BOOT_MAGIC, 4);
    BootBlock->LoaderVersion = OSLP_BOOT_VERSION;
    BootBlock->MemoryDescriptorListHead = MemoryDescriptorListHead;
    BootBlock->BootDriverListHead = ModuleListHead;
    BootBlock->AcpiTable = AcpiTable;
    BootBlock->AcpiTableVersion = AcpiTableVersion;
    BootBlock->BackBuffer = BackBuffer;
    BootBlock->FrontBuffer = FrontBuffer;
    BootBlock->FramebufferWidth = FramebufferWidth;
    BootBlock->FramebufferHeight = FramebufferHeight;
    BootBlock->FramebufferPitch = FramebufferPitch;

    /* All that's left before trying to transfer execution should be building the page map, so let's
     * leave that to the platform/arch specific function. */
    void *PageMap = OslpCreatePageMap(
        MemoryDescriptorListHead,
        &MemoryDescriptorStack,
        &LoadedPrograms,
        FramebufferHeight * FramebufferPitch * 4,
        BackBuffer);
    if (!PageMap) {
        return EFI_LOAD_ERROR;
    }

    OslpTransferExecution(
        BootBlock,
        (void *)((char *)BootStack + SIZE_8KB),
        PageMap,
        MemoryMapSize,
        MemoryMap,
        DescriptorSize,
        DescriptorVersion);
}
