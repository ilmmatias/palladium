/* SPDX-FileCopyrightText: (C) 2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <console.h>
#include <cpuid.h>
#include <loader.h>
#include <memory.h>
#include <platform.h>
#include <string.h>

uint64_t *OslpPageMap = NULL;
int OslpHasHugePages = 0;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function prepares the page map for a new mapping.
 *
 * PARAMETERS:
 *     Page - Current page of the range.
 *     Levels - How many levels we need to go down.
 *
 * RETURN VALUE:
 *     Pointer to the level you want to work on.
 *-----------------------------------------------------------------------------------------------*/
static uint64_t *PrepareMap(uint64_t Page, int Levels) {
    uint64_t *CurrentLevel = OslpPageMap;
    size_t Shift = 27;

    while (Levels--) {
        uint64_t Index = (Page >> Shift) & 0x1FF;

        if (!(CurrentLevel[Index] & 0x01)) {
            void *Address = OslAllocatePages(4096, PAGE_BOOT_TABLES);
            if (!Address) {
                return 0;
            }

            memset(Address, 0, 4096);
            CurrentLevel[Index] = (uint64_t)Address | 0x03;
        }

        CurrentLevel = (uint64_t *)(CurrentLevel[Index] & ~0xFFFull);
        Shift -= 9;
    }

    return CurrentLevel;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries mapping a huge (1GiB) page range, failing if we can't allocate the
 *     PDPT.
 *
 * PARAMETERS:
 *     BasePage - First page of the range.
 *     PageCount - How many pages the range has.
 *     TargetPage - Start of the target range.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
static int MapHugeRange(uint32_t BasePage, uint32_t PageCount, uint64_t TargetPage) {
    while (PageCount >= 0x40000) {
        uint64_t *Pdpt = PrepareMap(TargetPage, 1);
        if (!Pdpt) {
            return 0;
        }

        Pdpt[(TargetPage >> 18) & 0x1FF] = ((uint64_t)BasePage << 12) | 0x83;
        BasePage += 0x40000;
        PageCount -= 0x40000;
        TargetPage += 0x40000;
    }

    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries mapping a large (2MiB) page range, failing if we can't allocate the
 *     PDPT or the PDT.
 *
 * PARAMETERS:
 *     BasePage - First page of the range.
 *     PageCount - How many pages the range has.
 *     TargetPage - Start of the target range.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
static int MapLargeRange(uint32_t BasePage, uint32_t PageCount, uint64_t TargetPage) {
    while (PageCount >= 0x200) {
        uint64_t *Pdt = PrepareMap(TargetPage, 2);
        if (!Pdt) {
            return 0;
        }

        Pdt[(TargetPage >> 9) & 0x1FF] = ((uint64_t)BasePage << 12) | 0x83;
        BasePage += 0x200;
        PageCount -= 0x200;
        TargetPage += 0x200;
    }

    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries mapping a normal (4KiB) page range, failing if we can't allocate the
 *     PDPT, PDT, or PT entries.
 *
 * PARAMETERS:
 *     BasePage - First page of the range.
 *     PageCount - How many pages the range has.
 *     TargetPage - Start of the target range.
 *     Flags - Page flags; This will be OR'ed with 0x01 (present flag).
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
static int MapNormalRange(uint32_t BasePage, uint32_t PageCount, uint64_t TargetPage, int *Flags) {
    while (PageCount) {
        uint64_t *Pt = PrepareMap(TargetPage, 3);
        if (!Pt) {
            return 0;
        }

        uint64_t PageFlags;
        if (Flags) {
            if (*Flags & PAGE_FLAGS_WRITE) {
                PageFlags = 0x8000000000000003;
            } else if (!(*Flags & PAGE_FLAGS_EXEC)) {
                PageFlags = 0x8000000000000001;
            } else {
                PageFlags = 0x0000000000000001;
            }
        } else {
            PageFlags = 0x03;
        }

        Pt[TargetPage & 0x1FF] = ((uint64_t)BasePage << 12) | PageFlags;
        BasePage++;
        PageCount--;
        TargetPage++;

        if (Flags) {
            Flags++;
        }
    }

    return 1;
}

static int MapRange(uint32_t BasePage, uint32_t PageCount, uint64_t TargetPage, int *Flags) {
    if (!Flags) {
        /* Use as many 1GiB pages as possible. */
        if (OslpHasHugePages && !(TargetPage & 0x3FFFF) && PageCount >= 0x40000) {
            if (!MapHugeRange(BasePage, PageCount, TargetPage)) {
                return 0;
            }

            BasePage += PageCount & ~0x3FFFF;
            TargetPage += PageCount & ~0x3FFFF;
            PageCount -= PageCount & ~0x3FFFF;
        }

        /* Followed by large (2MiB) pages. */
        if (!(BasePage & 0x1FF) && PageCount >= 0x200) {
            if (!MapLargeRange(BasePage, PageCount, TargetPage)) {
                return 0;
            }

            BasePage += PageCount & ~0x1FF;
            TargetPage += PageCount & ~0x1FF;
            PageCount -= PageCount & ~0x1FF;
        }
    }

    /* Fallback to 4KiB pages if we have too few left/or if things weren't aligned properly. */
    return MapNormalRange(BasePage, PageCount, TargetPage, Flags);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function maps everything in the memory map to the fixed higher-half range, maps 1-to-1
 *     all OSLOADER ranges, and saves the new page map pointer for use by OslpTransferExecution.
 *
 * PARAMETERS:
 *     LoadedPrograms - Header of the loaded modules list.
 *     MemoryDescriptors - Header of the memory descriptors list.
 *
 * RETURN VALUE:
 *     1 if we're ready to transfer execution, 0 if we failed to setup.
 *-----------------------------------------------------------------------------------------------*/
int OslpPrepareExecution(RtDList *LoadedPrograms, RtDList *MemoryDescriptors) {
    OslpPageMap = OslAllocatePages(4096, PAGE_BOOT_TABLES);
    if (!OslpPageMap) {
        OslPrint("The system ran out of memory while creating the boot page map.\r\n");
        OslPrint("The boot process cannot continue.\r\n");
        return 0;
    }

    /* Check for 1GiB page support; If we do support it, it can reduces the amount of work to map
     * all ranges. */
    uint32_t Eax, Ebx, Ecx, Edx;
    __cpuid(0x80000001, Eax, Ebx, Ecx, Edx);
    OslpHasHugePages = Edx & 0x4000000;

    /* The last entry of the address space contains a self-reference (so that the
     * kernel can easily manipulate the page map). */
    memset(OslpPageMap, 0, 4096);
    OslpPageMap[511] = (uint64_t)OslpPageMap | 0x03;

    /* Map all of the memory descriptors. */
    for (RtDList *ListHeader = MemoryDescriptors->Next; ListHeader != MemoryDescriptors;) {
        OslpMemoryDescriptor *Descriptor =
            CONTAINING_RECORD(ListHeader, OslpMemoryDescriptor, ListHeader);
        ListHeader = ListHeader->Next;

        /* Map all physical memory into 0xFFFF800000000000, as read-write (only for the kernel
         * ofc). */
        if (!MapRange(
                Descriptor->BasePage,
                Descriptor->PageCount,
                0xFFFF800000000 + Descriptor->BasePage,
                NULL)) {
            OslPrint("The system ran out of memory while creating the boot page map.\r\n");
            OslPrint("The boot process cannot continue.\r\n");
            return 0;
        }
    }

    /* Followed by the loaded programs/module (they have a specific virtual address to be mapped
     * into). */
    for (RtDList *ListHeader = LoadedPrograms->Next; ListHeader != LoadedPrograms;
         ListHeader = ListHeader->Next) {
        OslpLoadedProgram *Module = CONTAINING_RECORD(ListHeader, OslpLoadedProgram, ListHeader);

        if (!MapRange(
                (uint64_t)Module->PhysicalAddress >> 12,
                (Module->ImageSize + 4095) >> 12,
                (uint64_t)Module->VirtualAddress >> 12,
                Module->PageFlags)) {
            OslPrint("The system ran out of memory while creating the boot page map.\r\n");
            OslPrint("The boot process cannot continue.\r\n");
            return 0;
        }
    }

    /* And to wrap up, identity map all regions that we might still use before jumping to the
     * kernel. */
    for (RtDList *ListHeader = MemoryDescriptors->Next; ListHeader != MemoryDescriptors;) {
        OslpMemoryDescriptor *Descriptor =
            CONTAINING_RECORD(ListHeader, OslpMemoryDescriptor, ListHeader);
        ListHeader = ListHeader->Next;

        if (Descriptor->Type != PAGE_OSLOADER && Descriptor->Type != PAGE_FIRMWARE_TEMPORARY) {
            continue;
        }

        if (!MapRange(Descriptor->BasePage, Descriptor->PageCount, Descriptor->BasePage, NULL)) {
            OslPrint("The system ran out of memory while creating the boot page map.\r\n");
            OslPrint("The boot process cannot continue.\r\n");
            return 0;
        }
    }

    return 1;
}
