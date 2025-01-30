/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <amd64/page.h>
#include <console.h>
#include <cpuid.h>
#include <loader.h>
#include <platform.h>
#include <string.h>

static bool HasHugePages = false;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets up the specified page frame to be present.
 *
 * PARAMETERS:
 *     Frame - Which frame to setup.
 *     Address - Which address to use.
 *     Flags - Which page flags to use.
 *     LargeLevel - Set to true if this needs the PageSize bit in the entry, or false otherwise.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void SetupFrame(PageFrame *Frame, uint64_t Address, int Flags, bool LargeLevel) {
    Frame->Present = 1;
    Frame->Address = Address >> 12;

    /* The "PageSize" bit becomes the PAT bit on the PT (last) level. */
    if (LargeLevel || (Flags & PAGE_FLAGS_DEVICE)) {
        Frame->PageSize = 1;
    }

    /* And bit 12 (which is available on the PT level) becomes the PAT bit on the other levels. */
    if (LargeLevel && (Flags & PAGE_FLAGS_DEVICE)) {
        Frame->Pat = 1;
    }

    /* W^X needs to be enforced on the caller, as we don't handle that here! */
    if (Flags & PAGE_FLAGS_WRITE) {
        Frame->Writable = 1;
    }

    if (!(Flags & PAGE_FLAGS_EXEC)) {
        Frame->NoExecute = 1;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks if we can add a page mapping of the given type/level, or if it would
 *     overwrite a pre-existing mapping.
 *
 * PARAMETERS:
 *     PageMap - Root PML4 pointer.
 *     VirtualAddress - Current target address.
 *     Levels - How many levels we need to go down.
 *
 * RETURN VALUE:
 *     true if the page frame already exists, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
static bool CheckLevel(PageFrame *PageMap, uint64_t VirtualAddress, int Levels) {
    size_t Shift = 39;

    while (Levels--) {
        uint64_t Index = (VirtualAddress >> Shift) & 511;

        /* Not present level is an instant return (no chance of anything being mapped here
         * already). */
        if (!PageMap[Index].Present) {
            return true;
        }

        /* And a large/huge page (when we don't want it) means we can't map anything here. */
        if (PageMap[Index].PageSize) {
            return false;
        }

        PageMap = (PageFrame *)(PageMap[Index].Address << 12);
        Shift -= 9;
    }

    return !PageMap[(VirtualAddress >> Shift) & 511].Present;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function prepares the page map for a new mapping.
 *
 * PARAMETERS:
 *     MemoryDescriptorListHead - Head of the memory descriptor list.
 *     MemoryDescriptorStack - Head of the descriptor entry stack.
 *     PageMap - Root PML4 pointer.
 *     VirtualAddress - Current target address.
 *     Levels - How many levels we need to go down.
 *
 * RETURN VALUE:
 *     Pointer to the level you want to work on.
 *-----------------------------------------------------------------------------------------------*/
static PageFrame *PrepareLevel(
    RtDList *MemoryDescriptorListHead,
    RtDList *MemoryDescriptorStack,
    PageFrame *PageMap,
    uint64_t VirtualAddress,
    int Levels) {
    size_t Shift = 39;

    while (Levels--) {
        uint64_t Index = (VirtualAddress >> Shift) & 511;

        if (!PageMap[Index].Present) {
            EFI_PHYSICAL_ADDRESS PhysicalAddress = 0;
            EFI_STATUS Status =
                gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &PhysicalAddress);
            if (Status != EFI_SUCCESS) {
                OslPrint("The system ran out of memory while creating the boot page map.\r\n");
                OslPrint("The boot process cannot continue.\r\n");
                return NULL;
            } else if (!OslpUpdateMemoryDescriptors(
                           MemoryDescriptorListHead,
                           MemoryDescriptorStack,
                           PAGE_TYPE_PAGE_MAP,
                           PhysicalAddress >> EFI_PAGE_SHIFT,
                           1)) {
                return NULL;
            }

            memset((void *)PhysicalAddress, 0, SIZE_4KB);
            SetupFrame(&PageMap[Index], PhysicalAddress, PAGE_FLAGS_WRITE | PAGE_FLAGS_EXEC, false);
        }

        PageMap = (PageFrame *)(PageMap[Index].Address << 12);
        Shift -= 9;
    }

    return PageMap;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries mapping a page at the specified level, failing if we can't allocate the
 *     one of the page levels.
 *
 * PARAMETERS:
 *     MemoryDescriptorListHead - Head of the memory descriptor list.
 *     MemoryDescriptorStack - Head of the descriptor entry stack.
 *     PageMap - Root PML4 pointer.
 *     VirtualAddress - Current address of the target range.
 *     PhysicalAddress - Current address of the source range.
 *     Flags - Page flags for the mapping.
 *     Levels - How many levels we need to go down.
 *     LargeLevel - Set to true if this needs the PageSize bit in the entry, or false otherwise.
 *     LevelShift - How many bits we need to shift the VirtualAddress to get the target level index.
 *
 * RETURN VALUE:
 *     true on success, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
static bool MapPage(
    RtDList *MemoryDescriptorListHead,
    RtDList *MemoryDescriptorStack,
    PageFrame *PageMap,
    uint64_t VirtualAddress,
    uint64_t PhysicalAddress,
    int Flags,
    int Levels,
    bool LargeLevel,
    uint64_t LevelShift) {
    PageFrame *TableLevel = PrepareLevel(
        MemoryDescriptorListHead, MemoryDescriptorStack, PageMap, VirtualAddress, Levels);
    if (!TableLevel) {
        return false;
    }

    PageFrame *Frame = &TableLevel[(VirtualAddress >> LevelShift) & 511];
    SetupFrame(Frame, PhysicalAddress, Flags, LargeLevel);
    return true;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function maps a range of physical addresses into virtual memory, using the largest
 *     possible page size.
 *
 * PARAMETERS:
 *     MemoryDescriptorListHead - Head of the memory descriptor list.
 *     MemoryDescriptorStack - Head of the descriptor entry stack.
 *     PageMap - Root PML4 pointer.
 *     VirtualAddress - Start address of the target range.
 *     PhysicalAddress - Start address of the source range.
 *     Size - How many bytes the range has.
 *     Flags - How we want to map the page.
 *
 * RETURN VALUE:
 *     true on success, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
static bool MapRange(
    RtDList *MemoryDescriptorListHead,
    RtDList *MemoryDescriptorStack,
    PageFrame *PageMap,
    uint64_t VirtualAddress,
    uint64_t PhysicalAddress,
    uint64_t Size,
    int Flags) {
    while (Size) {
        /* Use as many 1GiB pages as possible. */
        bool UseHugePage = HasHugePages && CheckLevel(PageMap, VirtualAddress, 1) &&
                           !(VirtualAddress & (SIZE_1GB - 1)) &&
                           !(PhysicalAddress & (SIZE_1GB - 1)) && Size >= SIZE_1GB;
        if (UseHugePage) {
            if (!MapPage(
                    MemoryDescriptorListHead,
                    MemoryDescriptorStack,
                    PageMap,
                    VirtualAddress,
                    PhysicalAddress,
                    Flags,
                    1,
                    true,
                    30)) {
                return false;
            }

            VirtualAddress += SIZE_1GB;
            PhysicalAddress += SIZE_1GB;
            Size -= SIZE_1GB;
            continue;
        }

        /* Followed by large (2MiB) pages. */
        bool UseLargePage = CheckLevel(PageMap, VirtualAddress, 2) &&
                            !(VirtualAddress & (SIZE_2MB - 1)) &&
                            !(PhysicalAddress & (SIZE_2MB - 1)) && Size >= SIZE_2MB;
        if (UseLargePage) {
            if (!MapPage(
                    MemoryDescriptorListHead,
                    MemoryDescriptorStack,
                    PageMap,
                    VirtualAddress,
                    PhysicalAddress,
                    Flags,
                    2,
                    true,
                    21)) {
                return false;
            }

            VirtualAddress += SIZE_2MB;
            PhysicalAddress += SIZE_2MB;
            Size -= SIZE_2MB;
            continue;
        }

        /* Fallback to 4KiB pages if we have too few left/or if things weren't aligned properly. */
        bool UseSmallPage = CheckLevel(PageMap, VirtualAddress, 3);
        if (UseSmallPage) {
            if (!MapPage(
                    MemoryDescriptorListHead,
                    MemoryDescriptorStack,
                    PageMap,
                    VirtualAddress,
                    PhysicalAddress,
                    Flags,
                    3,
                    false,
                    12)) {
                return false;
            }
        }

        VirtualAddress += SIZE_4KB;
        PhysicalAddress += SIZE_4KB;
        Size -= SIZE_4KB;
    }

    return true;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates the page map, and maps everything we need into memory (loaded
 *     programs/modules, frontbuffer, backbuffer, etc).
 *
 * PARAMETERS:
 *     MemoryDescriptorListHead - Head of the memory descriptor list.
 *     MemoryDescriptorStack - Head of the descriptor entry stack.
 *     LoadedPrograms - Head of the loaded programs list; We need this to get the page flags.
 *     BackBuffer - Physical address of the back buffer.
 *
 * RETURN VALUE:
 *     Either the allocated page map, or NULL if we failed.
 *-----------------------------------------------------------------------------------------------*/
void *OslpCreatePageMap(
    RtDList *MemoryDescriptorListHead,
    RtDList *MemoryDescriptorStack,
    RtDList *LoadedPrograms,
    uint64_t FrameBufferSize,
    void *BackBuffer) {
    EFI_PHYSICAL_ADDRESS PhysicalAddress = 0;
    EFI_STATUS Status = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &PhysicalAddress);
    if (Status != EFI_SUCCESS) {
        OslPrint("The system ran out of memory while creating the boot page map.\r\n");
        OslPrint("The boot process cannot continue.\r\n");
        return NULL;
    } else if (!OslpUpdateMemoryDescriptors(
                   MemoryDescriptorListHead,
                   MemoryDescriptorStack,
                   PAGE_TYPE_PAGE_MAP,
                   PhysicalAddress >> EFI_PAGE_SHIFT,
                   1)) {
        return NULL;
    }

    /* Check for 1GiB page support; If we do support it, it can reduces the amount of work to map
     * all ranges. */
    uint32_t Eax, Ebx, Ecx, Edx;
    __cpuid(0x80000001, Eax, Ebx, Ecx, Edx);
    HasHugePages = (Edx & 0x4000000) != 0;

    /* The last entry of the address space contains a self-reference (so that the
     * kernel can easily manipulate the page map). */
    PageFrame *PageMap = (PageFrame *)PhysicalAddress;
    memset(PageMap, 0, SIZE_4KB);
    SetupFrame(&PageMap[511], (uint64_t)PageMap, PAGE_FLAGS_WRITE, false);

    /* Identity map all descriptors that are either OSLOADER (because we need them while we change
     * CR3) or FIRMWARE (we need them for SetVirtualAddressMap). */
    for (RtDList *ListHeader = MemoryDescriptorListHead->Next;
         ListHeader != MemoryDescriptorListHead;
         ListHeader = ListHeader->Next) {
        OslpMemoryDescriptor *Descriptor =
            CONTAINING_RECORD(ListHeader, OslpMemoryDescriptor, ListHeader);

        if (Descriptor->Type != PAGE_TYPE_OSLOADER_TEMPORARY &&
            Descriptor->Type != PAGE_TYPE_FIRMWARE_TEMPORARY &&
            Descriptor->Type != PAGE_TYPE_FIRMWARE_PERMANENT) {
            continue;
        }

        if (!MapRange(
                MemoryDescriptorListHead,
                MemoryDescriptorStack,
                PageMap,
                Descriptor->BasePage << EFI_PAGE_SHIFT,
                Descriptor->BasePage << EFI_PAGE_SHIFT,
                Descriptor->PageCount << EFI_PAGE_SHIFT,
                PAGE_FLAGS_WRITE | PAGE_FLAGS_EXEC)) {
            OslPrint("The system ran out of memory while creating the boot page map.\r\n");
            OslPrint("The boot process cannot continue.\r\n");
            return NULL;
        }
    }

    /* Map all FIRMWARE descriptors into high memory (as read+write+exec). */
    for (RtDList *ListHeader = MemoryDescriptorListHead->Next;
         ListHeader != MemoryDescriptorListHead;
         ListHeader = ListHeader->Next) {
        OslpMemoryDescriptor *Descriptor =
            CONTAINING_RECORD(ListHeader, OslpMemoryDescriptor, ListHeader);

        if (Descriptor->Type != PAGE_TYPE_FIRMWARE_PERMANENT) {
            continue;
        }

        if (!MapRange(
                MemoryDescriptorListHead,
                MemoryDescriptorStack,
                PageMap,
                0xFFFF800000000000 + (Descriptor->BasePage << EFI_PAGE_SHIFT),
                Descriptor->BasePage << EFI_PAGE_SHIFT,
                Descriptor->PageCount << EFI_PAGE_SHIFT,
                PAGE_FLAGS_WRITE | PAGE_FLAGS_EXEC)) {
            OslPrint("The system ran out of memory while creating the boot page map.\r\n");
            OslPrint("The boot process cannot continue.\r\n");
            return NULL;
        }
    }

    /* And map all other memory descriptor/physical memory areas, but as read+write-only. */
    for (RtDList *ListHeader = MemoryDescriptorListHead->Next;
         ListHeader != MemoryDescriptorListHead;
         ListHeader = ListHeader->Next) {
        OslpMemoryDescriptor *Descriptor =
            CONTAINING_RECORD(ListHeader, OslpMemoryDescriptor, ListHeader);

        if (Descriptor->Type == PAGE_TYPE_FIRMWARE_PERMANENT) {
            continue;
        }

        if (!MapRange(
                MemoryDescriptorListHead,
                MemoryDescriptorStack,
                PageMap,
                0xFFFF800000000000 + (Descriptor->BasePage << EFI_PAGE_SHIFT),
                Descriptor->BasePage << EFI_PAGE_SHIFT,
                Descriptor->PageCount << EFI_PAGE_SHIFT,
                PAGE_FLAGS_WRITE)) {
            OslPrint("The system ran out of memory while creating the boot page map.\r\n");
            OslPrint("The boot process cannot continue.\r\n");
            return NULL;
        }
    }

    /* Map all loaded programs and modules (while taking caution with the page flags!). */
    for (RtDList *ListHeader = LoadedPrograms->Next; ListHeader != LoadedPrograms;
         ListHeader = ListHeader->Next) {
        OslpLoadedProgram *Module = CONTAINING_RECORD(ListHeader, OslpLoadedProgram, ListHeader);

        uint64_t VirtualAddress = (uint64_t)Module->VirtualAddress;
        uint64_t PhysicalAddress = (uint64_t)Module->PhysicalAddress;
        uint64_t ImageSize = Module->ImageSize;
        int *PageFlags = Module->PageFlags;

        while (ImageSize) {
            uint64_t SectionSize = SIZE_4KB;
            int Flags = *(PageFlags++);

            while (SectionSize < ImageSize) {
                if (Flags != *PageFlags) {
                    break;
                }

                SectionSize += SIZE_4KB;
                PageFlags++;
            }

            if (!MapRange(
                    MemoryDescriptorListHead,
                    MemoryDescriptorStack,
                    PageMap,
                    VirtualAddress,
                    PhysicalAddress,
                    SectionSize,
                    Flags)) {
                OslPrint("The system ran out of memory while creating the boot page map.\r\n");
                OslPrint("The boot process cannot continue.\r\n");
                return NULL;
            }

            VirtualAddress += SectionSize;
            PhysicalAddress += SectionSize;
            ImageSize -= SectionSize;
        }
    }

    /* Map the display's back buffer (aligning the size up to the nearest 2MiB, or the nearest 1GiB
     * if huge pages are available). */
    uint64_t BackBufferSize = HasHugePages ? (FrameBufferSize + SIZE_1GB - 1) & ~(SIZE_1GB - 1)
                                           : (FrameBufferSize + SIZE_2MB - 1) & ~(SIZE_2MB - 1);

    if (!MapRange(
            MemoryDescriptorListHead,
            MemoryDescriptorStack,
            PageMap,
            0xFFFF800000000000 + (uint64_t)BackBuffer,
            (uint64_t)BackBuffer,
            BackBufferSize,
            PAGE_FLAGS_WRITE | PAGE_FLAGS_DEVICE)) {
        OslPrint("The system ran out of memory while creating the boot page map.\r\n");
        OslPrint("The boot process cannot continue.\r\n");
        return NULL;
    }

    return PageMap;
}
