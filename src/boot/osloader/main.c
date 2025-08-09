/* SPDX-FileCopyrightText: (C) 2024-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <acpi.h>
#include <config.h>
#include <console.h>
#include <ctype.h>
#include <entropy.h>
#include <file.h>
#include <graphics.h>
#include <loader.h>
#include <os/intrin.h>
#include <platform.h>
#include <stdio.h>
#include <string.h>
#include <support.h>

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
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
EFI_STATUS OslMain(EFI_HANDLE *ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    do {
        /* Save up any required EFI variables (so that we don't need to pass ImageHandle and
         * SystemTable around). */
        gIH = ImageHandle;
        gST = SystemTable;
        gBS = SystemTable->BootServices;
        gRT = SystemTable->RuntimeServices;

        /* Get rid of the watchdog timer (just to be sure). */
        gBS->SetWatchdogTimer(0, 0, 0, NULL);

        /* Run the basic compatibility checks against the host system. */
        if (!OslpCheckArchSupport()) {
            break;
        }

        /* Get the RNG ready for randomizing the virtual memory load addresses. */
        OslpInitializeEntropy();

        /* Initialize all required subsystems. */
        EFI_STATUS Status = OslpInitializeRootVolume();
        if (Status != EFI_SUCCESS) {
            break;
        }

        Status = OslpInitializeVirtualAllocator();
        if (Status != EFI_SUCCESS) {
            break;
        }

        void *AcpiRootPointer = NULL;
        void *AcpiRootTable = NULL;
        uint32_t AcpiRootTableSize = 0;
        uint32_t AcpiVersion = 0;
        if (!OslpInitializeAcpi(
                &AcpiRootPointer, &AcpiRootTable, &AcpiRootTableSize, &AcpiVersion)) {
            break;
        }

        void *BackBufferPhys = NULL;
        void *BackBufferVirt = NULL;
        void *FrontBufferPhys = NULL;
        void *FrontBufferVirt = NULL;
        uint32_t FramebufferWidth = 0;
        uint32_t FramebufferHeight = 0;
        uint32_t FramebufferPitch = 0;
        Status = OslpInitializeGraphics(
            &BackBufferPhys,
            &BackBufferVirt,
            &FrontBufferPhys,
            &FrontBufferVirt,
            &FramebufferWidth,
            &FramebufferHeight,
            &FramebufferPitch);
        if (Status != EFI_SUCCESS) {
            break;
        }

        /* Let's get the actual boot process started; There should be a configuration file in the
         * fixed/known path telling us what we need to load. */
        OslConfig Config;
        if (!OslLoadConfigFile("\\EFI\\PALLADIUM\\BOOT.CFG", &Config)) {
            break;
        }

        RtDList LoadedPrograms;
        RtInitializeDList(&LoadedPrograms);

        /* The parser gives us only the relative paths, so we need to manually mount the full path
         * for the kernel (using the known/default prefix). */
        size_t ModuleNameLength = strlen(Config.Kernel);
        char *ModulePath = NULL;
        Status = gBS->AllocatePool(EfiLoaderData, ModuleNameLength + 16, (VOID **)&ModulePath);
        if (Status != EFI_SUCCESS) {
            OslPrint("Failed to load a kernel/driver file.\r\n");
            OslPrint("The system ran out of memory while loading %s.\r\n", Config.Kernel);
            OslPrint("The boot process cannot continue.\r\n");
            break;
        } else {
            snprintf(ModulePath, ModuleNameLength + 16, "\\EFI\\PALLADIUM\\%s", Config.Kernel);
        }

        /* The module name is fixed though (always kernel.exe, as that's what every driver
         * expects). */
        if (!OslLoadExecutable(&LoadedPrograms, "kernel.exe", ModulePath)) {
            break;
        }

        /* The boot drivers do need some extra work for both the full path (just like the kernel)
         * and for the module name (always the lowercase version of the given name). */
        bool Failed = false;
        for (size_t i = 0; i < Config.BootDriverCount; i++) {
            ModuleNameLength = strlen(Config.BootDrivers[i]);
            Status = gBS->AllocatePool(EfiLoaderData, ModuleNameLength + 16, (VOID **)&ModulePath);
            if (Status != EFI_SUCCESS) {
                OslPrint("Failed to load a kernel/driver file.\r\n");
                OslPrint(
                    "The system ran out of memory while loading %s.\r\n", Config.BootDrivers[i]);
                OslPrint("The boot process cannot continue.\r\n");
                Failed = true;
                break;
            }

            /* As the received module name is already in all caps, we can just append it to the full
             * path. */
            snprintf(
                ModulePath, ModuleNameLength + 16, "\\EFI\\PALLADIUM\\%s", Config.BootDrivers[i]);

            /* And then follow by making it lowercase (to use as the module name we'll pass to the
             * kernel). */
            for (size_t j = 0; j < ModuleNameLength; j++) {
                Config.BootDrivers[i][j] = tolower(Config.BootDrivers[i][j]);
            }

            if (!OslLoadExecutable(&LoadedPrograms, Config.BootDrivers[i], ModulePath)) {
                Failed = true;
                break;
            }
        }

        /* Maybe we should just goto instead of doing this flag thingy? */
        if (Failed) {
            break;
        }

        /* Validate that no invalid imports exist. */
        if (!OslFixupImports(&LoadedPrograms)) {
            break;
        }

        /* And wrap up by relocating tthe base address of all modules (from their desired base to
         * our chosen virtual address). */
        OslFixupRelocations(&LoadedPrograms);

        /* Create the target/kernel module entry list (this is what the kernel will have access, as
         * the LoadedPrograms list is internal to us). */
        RtDList *ModuleListHead = OslCreateKernelModuleList(&LoadedPrograms);
        if (!ModuleListHead) {
            break;
        }

        /* We need to create a small (8KiB) temporary stack (for use during the kernel BSP
         * initialization, as the current UEFI stack probably won't be mapped in). */
        void *BootStack = OslAllocatePages(SIZE_8KB, SIZE_4KB);
        if (!BootStack) {
            OslPrint("Failed to allocate space for the boot stack.\r\n");
            OslPrint("The boot process cannot continue.\r\n");
            break;
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
                &LoadedPrograms,
                FrontBufferPhys,
                FramebufferHeight * FramebufferPitch * 4,
                &MemoryDescriptorListHead,
                &MemoryDescriptorStack,
                &MemoryMapSize,
                &MemoryMap,
                &DescriptorSize,
                &DescriptorVersion)) {
            break;
        }

        /* We can now wrap up by filling the boot block, and creating the page map. */
        OslpBootBlock *BootBlock = NULL;
        Status = gBS->AllocatePool(EfiLoaderData, sizeof(OslpBootBlock), (VOID **)&BootBlock);
        if (Status != EFI_SUCCESS) {
            OslPrint("Failed to allocate space for the boot block.\r\n");
            OslPrint("The boot process cannot continue.\r\n");
            break;
        }

        /* All important allocations from now on need to update the descriptor list manually! */
        if (!OslpUpdateMemoryDescriptors(
                MemoryDescriptorListHead,
                &MemoryDescriptorStack,
                PAGE_TYPE_OSLOADER_TEMPORARY,
                (uint64_t)BootBlock >> EFI_PAGE_SHIFT,
                (sizeof(OslpBootBlock) + EFI_PAGE_SIZE - 1) >> EFI_PAGE_SHIFT)) {
            break;
        }

        memcpy(BootBlock->Basic.Magic, OSLP_BOOT_MAGIC, 4);
        BootBlock->Basic.LoaderVersion = OSLP_BOOT_VERSION;
        BootBlock->Basic.MemoryDescriptorListHead = MemoryDescriptorListHead;
        BootBlock->Basic.BootDriverListHead = ModuleListHead;
        BootBlock->Acpi.RootPointer = AcpiRootPointer;
        BootBlock->Acpi.RootTable = AcpiRootTable;
        BootBlock->Acpi.RootTableSize = AcpiRootTableSize;
        BootBlock->Acpi.Version = AcpiVersion;
        BootBlock->Graphics.BackBuffer = BackBufferVirt;
        BootBlock->Graphics.FrontBuffer = FrontBufferVirt;
        BootBlock->Graphics.Width = FramebufferWidth;
        BootBlock->Graphics.Height = FramebufferHeight;
        BootBlock->Graphics.Pitch = FramebufferPitch;
        OslpInitializeArchBootData(BootBlock);

        /* All that's left before trying to transfer execution should be building the page map, so
         * let's leave that to the platform/arch specific function. */
        void *PageMap = OslpCreatePageMap(
            MemoryDescriptorListHead,
            &MemoryDescriptorStack,
            &LoadedPrograms,
            FramebufferHeight * FramebufferPitch * 4,
            BackBufferPhys,
            BackBufferVirt,
            FrontBufferPhys,
            FrontBufferVirt);
        if (!PageMap) {
            break;
        }

        OslpTransferExecution(
            BootBlock,
            (void *)((char *)BootStack + SIZE_8KB),
            PageMap,
            MemoryMapSize,
            MemoryMap,
            DescriptorSize,
            DescriptorVersion);
    } while (false);

    while (true) {
        StopProcessor();
    }
}
