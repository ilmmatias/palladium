/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <boot.h>
#include <display.h>
#include <memory.h>
#include <string.h>
#include <x86/bios.h>
#include <x86/cpuid.h>
#include <x86/idt.h>

extern uint64_t BiosRsdtLocation;
extern int BiosTableType;

extern BiMemoryDescriptor BiMemoryDescriptors[BI_MAX_MEMORY_DESCRIPTORS];
extern int BiMemoryDescriptorCount;
extern uint64_t BiUsableMemorySize;
extern uint64_t BiMaxAdressableMemory;

extern char *BiVideoBuffer;
extern uint16_t BiVideoWidth;
extern uint16_t BiVideoHeight;
extern uint16_t BiVideoPitch;

[[noreturn]] void
BiJumpPalladium(uint64_t *Pml4, uint64_t BootData, uint64_t EntryPoint, uint64_t ProcessorStruct);

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets up a new handler within the IDT.
 *
 * PARAMETERS:
 *     Number - Index within the IDT to setup the handler at.
 *     Handler - Virtual address of the handler function.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void InstallIdtHandler(int Number, uint64_t Handler) {
    IdtDescs[Number].OffsetLow = Handler;
    IdtDescs[Number].Segment = 0x08;
    IdtDescs[Number].Ist = 0;
    IdtDescs[Number].TypeAttributes = 0x8E;
    IdtDescs[Number].OffsetMid = Handler >> 16;
    IdtDescs[Number].OffsetHigh = Handler >> 32;
    IdtDescs[Number].Reserved = 0;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets up the paging structures, and prepares to put the processor in the
 *     correct state to jump into the kernel.
 *
 * PARAMETERS:
 *     Images - List containing all memory regions we need to map.
 *     ImageCount - How many entries the image list has.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void BiStartPalladium(LoadedImage *Images, size_t ImageCount) {
    /* TODO: Add support for PML5 (under compatible processors). */

    /* Check for 1GiB page support; If we do support it, it reduces the amount of work to map all
       of the physical memory. */
    uint32_t Eax, Ebx, Ecx, Edx;
    __cpuid(0x80000001, Eax, Ebx, Ecx, Edx);

    /* Pre-allocate the space required for the kernel's physical memory manager; This way
       the kernel doesn't need to worry about anything but filling the info.
       MmSize should always be `PagesOfMemory * sizeof(MiPageEntry)`, if MiPageEntry changes
       on the kernel, the size here needs to change too! */
    uint64_t PagesOfMemory = (BiUsableMemorySize + BI_PAGE_SIZE - 1) >> BI_PAGE_SHIFT;
    void *MmBase = BmAllocatePages(PagesOfMemory * 32, BM_MD_KERNEL);
    if (!MmBase) {
        BmPrint("Could not allocate enough memory for the memory manager.\n"
                "Your system might not have enough usable memory.\n");
        while (1)
            ;
    }

    /* Pre-allocate the pool bitmap as well (so that the kernel can initialize everything
       without trashing the loader struct before driver initialization). */
    uint64_t PoolSize = 0x2000000000;
    void *PoolBitmapBase = BmAllocatePages((PoolSize >> BI_PAGE_SHIFT) >> 3, BM_MD_KERNEL);
    if (!PoolBitmapBase) {
        BmPrint("Could not allocate enough memory for the memory manager.\n"
                "Your system might not have enough usable memory.\n");
        while (1)
            ;
    }

    /* A double buffer is required so that we can implement scrolling on the boot terminal
       without reading the (really slow) back buffer. */
    void *ScreenFrontBase = BmAllocatePages(BiVideoWidth * BiVideoHeight * 4, BM_MD_KERNEL);
    if (!ScreenFrontBase) {
        BmPrint("Could not allocate enough memory for the screen front buffer.\n"
                "Your system might not have enough usable memory.\n");
        while (1)
            ;
    }

    uint64_t Slices1GiB = (BiMaxAdressableMemory + 0x3FFFFFFF) >> 30;
    uint64_t Slices2MiB = (BiMaxAdressableMemory + 0x1FFFFF) >> 21;

    /* We're mapping at most 512GiB over here, the kernel should map everything else. */
    if (Slices1GiB > 512) {
        Slices1GiB = 512;
    }

    if (Slices2MiB > 262144) {
        Slices2MiB = 262144;
    }

    do {
        int Fail = 0;
        uint64_t *Pml4 = BmAllocatePages(BI_PAGE_SIZE, BM_MD_KERNEL);
        uint64_t *EarlyIdentPdpt = BmAllocatePages(BI_PAGE_SIZE, BM_MD_KERNEL);
        uint64_t *LateIdentPdpt = BmAllocatePages(BI_PAGE_SIZE, BM_MD_KERNEL);
        uint64_t *KernelPdpt = BmAllocatePages(BI_PAGE_SIZE, BM_MD_KERNEL);
        uint64_t *EarlyIdentPdt = BmAllocatePages(BI_PAGE_SIZE, BM_MD_KERNEL);
        LoaderBootData *BootData = BmAllocateBlock(sizeof(LoaderBootData));

        if (!Pml4 || !EarlyIdentPdpt || !LateIdentPdpt || !EarlyIdentPdt || !KernelPdpt ||
            !BootData) {
            break;
        }

        /* First 2MiB are the early identity mapping (which contains ourselves; Required for no
           crash upon entering long mode).
           Right after the higher half barrier, we have the actual identity mapping (supporting
           up to 16TiB). We map only 512GiB, and the kernel should map anything else after. The
           kernel and the kernel drivers are mapped at last, in whichever locations the ASLR
           layer chose.
           The last entry of the address space contains a self-reference (so that the kernel can
           easily manipulate the page map). */
        memset(Pml4, 0, BI_PAGE_SIZE);
        Pml4[0] = (uint64_t)EarlyIdentPdpt | 0x03;
        Pml4[256] = (uint64_t)LateIdentPdpt | 0x03;
        Pml4[(BI_ARENA_BASE >> 39) & 0x1FF] = (uint64_t)KernelPdpt | 0x03;
        Pml4[511] = (uint64_t)Pml4 | 0x03;

        if (Edx & bit_PDPE1GB) {
            BmPrint("mapping %llu 1GiB slices of adressable physical memory\n", Slices1GiB);
            memset(LateIdentPdpt, 0, BI_PAGE_SIZE);
            for (uint64_t i = 0; i < Slices1GiB; i++) {
                LateIdentPdpt[i] = (i << 30) | 0x83;
            }
        } else {
            uint64_t *LateIdentPdt = BmAllocatePages(Slices1GiB << BI_PAGE_SHIFT, BM_MD_KERNEL);
            if (!LateIdentPdt) {
                break;
            }

            memset(LateIdentPdpt, 0, BI_PAGE_SIZE);
            for (uint64_t i = 0; i < Slices1GiB; i++) {
                LateIdentPdpt[i] = (uint64_t)(LateIdentPdt + (i << 9)) | 0x03;
            }

            BmPrint("mapping %llu 2MiB slices of adressable physical memory\n", Slices2MiB);
            memset(LateIdentPdt, 0, Slices1GiB * BI_PAGE_SIZE);
            for (uint64_t i = 0; i < Slices2MiB; i++) {
                LateIdentPdt[i] = (i << 21) | 0x83;
            }
        }

        memset(EarlyIdentPdpt, 0, BI_PAGE_SIZE);
        EarlyIdentPdpt[0] = (uint64_t)EarlyIdentPdt | 0x03;

        memset(EarlyIdentPdt, 0, BI_PAGE_SIZE);
        EarlyIdentPdt[0] = 0x83;

        memset(KernelPdpt, 0, BI_PAGE_SIZE);
        for (size_t i = 0; i < ImageCount; i++) {
            /* 1GiB should be a sane limit for a single image/driver. */
            uint64_t ImageSlices2MiB = (Images[i].ImageSize + 0x1FFFFF) >> 21;
            uint64_t ImageSlices4KiB = Images[i].ImageSize >> 12;

            uint64_t ImagePdptBase = (Images[i].VirtualAddress >> 30) & 0x1FF;
            uint64_t ImagePdtBase = (Images[i].VirtualAddress >> 21) & 0x1FF;
            uint64_t ImagePtBase = (Images[i].VirtualAddress >> 12) & 0x1FF;

            /* Take a lot of caution with the virtual address, if we straddle the 2MiB barrier,
               we need one extra large slice. */
            if (ImagePdtBase !=
                (((Images[i].VirtualAddress + (ImageSlices4KiB << 12)) >> 21) & 0x1FF)) {
                ImageSlices2MiB++;
            }

            uint64_t *ImagePdt = BmAllocatePages(BI_PAGE_SIZE, BM_MD_KERNEL);
            uint64_t *ImagePt = BmAllocatePages(ImageSlices2MiB << BI_PAGE_SHIFT, BM_MD_KERNEL);

            if (!ImagePdt || !ImagePt) {
                break;
            }

            KernelPdpt[ImagePdptBase] = (uint64_t)ImagePdt | 0x03;

            memset(ImagePdt, 0, BI_PAGE_SIZE);
            for (uint64_t j = 0; j < ImageSlices2MiB; j++) {
                ImagePdt[ImagePdtBase + j] = (uint64_t)(ImagePt + (j << 9)) | 0x03;
            }

            BmPrint("mapping %llu slices of 4KiB of image %zu\n", ImageSlices4KiB, i);
            memset(ImagePt, 0, ImageSlices2MiB * BI_PAGE_SIZE);
            for (uint64_t j = 0; j < ImageSlices4KiB; j++) {
                int Flags = Images[i].PageFlags[j];
                ImagePt[ImagePtBase + j] = (uint64_t)(Images[i].PhysicalAddress + (j << 12)) |
                                           (Flags & PAGE_WRITE     ? 0x8000000000000003
                                            : !(Flags & PAGE_EXEC) ? 0x8000000000000001
                                                                   : 0x01);
            }
        }

        if (Fail) {
            break;
        }

        /* Setup the IDT, mapping all exception handlers properly, and anything higher than
           32 (aka not an exception) to IRQ 0. */

        for (int i = 0; i < 32; i++) {
            InstallIdtHandler(i, IrqStubTable[i]);
        }

        for (int i = 32; i < 256; i++) {
            InstallIdtHandler(i, IrqStubTable[32]);
        }

        /* Mount the OS-specific boot data, and enter assembly land to get into long mode.  */

        memcpy(BootData->Magic, LOADER_MAGIC, 4);
        BootData->Version = LOADER_CURRENT_VERSION;
        BootData->Acpi.BaseAdress = BiosRsdtLocation;
        BootData->Acpi.TableType = BiosTableType;
        BootData->MemoryManager.MemorySize = BiUsableMemorySize;
        BootData->MemoryManager.PageAllocatorBase = (uint64_t)MmBase + 0xFFFF800000000000;
        BootData->MemoryManager.PoolBitmapBase = (uint64_t)PoolBitmapBase + 0xFFFF800000000000;
        BootData->MemoryMap.BaseAddress = (uint64_t)BiMemoryDescriptors + 0xFFFF800000000000;
        BootData->MemoryMap.Count = BiMemoryDescriptorCount;
        BootData->Display.BackBufferBase = (uint64_t)BiVideoBuffer + 0xFFFF800000000000;
        BootData->Display.FrontBufferBase = (uint64_t)ScreenFrontBase + 0xFFFF800000000000;
        BootData->Display.Width = BiVideoWidth;
        BootData->Display.Height = BiVideoHeight;
        BootData->Display.Pitch = BiVideoPitch;
        BootData->Images.BaseAddress = (uint64_t)Images + 0xFFFF800000000000;
        BootData->Images.Count = ImageCount;

        /* The loader should have placed the stack directly above the kernel image. */
        BiJumpPalladium(
            Pml4,
            (uint64_t)BootData + 0xFFFF800000000000,
            Images[0].EntryPoint,
            Images[0].VirtualAddress + Images[0].ImageSize - SIZEOF_PROCESSOR);
    } while (0);

    BmPrint("Something went wrong while loading the OS.\n"
            "Your system might not have enough usable memory.\n");
    while (1)
        ;
}
