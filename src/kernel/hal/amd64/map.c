/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/ke.h>
#include <kernel/mi.h>
#include <string.h>

extern bool HalpSmpInitializationComplete;

typedef struct {
    bool ReloadCr3;
    uint64_t Start;
    uint64_t End;
} IpiContext;

/* Constants related to each page table level. */
typedef struct {
    HalpPageFrame *Base;
    uint64_t Shift;
    uint64_t Mask;
    uint64_t Size;
} TableLevel;

static const TableLevel TableLevels[] = {
    {HALP_PML4_BASE, HALP_PML4_SHIFT, HALP_PML4_MASK, HALP_PML4_SIZE},
    {HALP_PDPT_BASE, HALP_PDPT_SHIFT, HALP_PDPT_MASK, HALP_PDPT_SIZE},
    {HALP_PD_BASE, HALP_PD_SHIFT, HALP_PD_MASK, HALP_PD_SIZE},
    {HALP_PT_BASE, HALP_PT_SHIFT, HALP_PT_MASK, HALP_PT_SIZE},
};

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function builds a new page frame in-place using the specified flags.
 *
 * PARAMETERS:
 *     Frame - Where to store the built frame.
 *     Source - Current physical address.
 *     Level - Current table level data.
 *     Flags - Flags for the frame.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void BuildFrame(HalpPageFrame *Frame, uint64_t Source, uint64_t Level, int Flags) {
    Frame->RawData = 0;
    Frame->Present = 1;
    Frame->Writable = (Flags & MI_MAP_WRITE) != 0;
    Frame->Address = Source >> HALP_PT_SHIFT;
    Frame->NoExecute = (Flags & MI_MAP_EXEC) == 0;

    /* The "PageSize" bit becomes the PAT bit on the PT (last) level. */
    if (Level != HALP_PT_LEVEL || (Flags & MI_MAP_WC)) {
        Frame->PageSize = 1;
    }

    /* And bit 12 (which is available on the PT level) becomes the PAT bit on the other levels. */
    if (Level != HALP_PT_LEVEL && (Flags & MI_MAP_WC)) {
        Frame->Pat = 1;
    } else if (Flags & MI_MAP_UC) {
        Frame->CacheDisable = 1;
        Frame->WriteThrough = 1;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function traverses a single level of the page table, allocating the specified entry if
 *     required. This should not be used to traverse/map the target level itself (only the
 *     containing levels).
 *
 * PARAMETERS:
 *     CurrentFrame - Pointer to where the wanted frame in the current level is.
 *     NextFrame - Pointer to the where the wanted frame will be in the next level.
 *
 * RETURN VALUE:
 *     true on success, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
static bool AllocateFrame(HalpPageFrame *CurrentFrame, HalpPageFrame *NextFrame) {
    if (CurrentFrame->Present) {
        return true;
    }

    /* Allocate a new page frame; This shouldn't require any invlpg/TLB shootdown. */
    uint64_t Page = MiPageList ? MmAllocateSinglePage() : MiAllocateEarlyPages(1);
    if (!Page) {
        return false;
    }

    CurrentFrame->RawData = 0;
    CurrentFrame->Present = 1;
    CurrentFrame->Writable = 1;
    CurrentFrame->Address = (uint64_t)Page >> HALP_PT_SHIFT;

    /* And clean up the child level via the recursive paging (so that we don't need the allocated
     * page to be mapped into higher memory). */
    memset((void *)((uint64_t)NextFrame & ~(HALP_PT_SIZE - 1)), 0, HALP_PT_SIZE);
    return true;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function traverses the page table, without trying to allocate non-present levels.
 *
 * PARAMETERS:
 *     VirtualAddress - Virtual address to process.
 *     TargetLevel - Output; Will contain the number of the reached table level.
 *     TargetFrame - Output; Will contain a pointer to the entry.
 *
 * RETURN VALUE:
 *     true if all levels along the way were present, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
static bool GetFrame(uint64_t VirtualAddress, uint64_t *TargetLevel, HalpPageFrame **TargetFrame) {
    /* Extending this to PML5 should be as easy as handling the PML5 level the same as PML4. */
    HalpPageFrame *Frame = &HALP_PML4_BASE[(VirtualAddress >> HALP_PML4_SHIFT) & HALP_PML4_MASK];
    if (!Frame->Present) {
        *TargetLevel = HALP_PML4_LEVEL;
        return false;
    }

    Frame = &HALP_PDPT_BASE[(VirtualAddress >> HALP_PDPT_SHIFT) & HALP_PDPT_MASK];
    if (!Frame->Present) {
        *TargetLevel = HALP_PDPT_LEVEL;
        return false;
    } else if (Frame->PageSize) {
        *TargetLevel = HALP_PDPT_LEVEL;
        *TargetFrame = Frame;
        return true;
    }

    Frame = &HALP_PD_BASE[(VirtualAddress >> HALP_PD_SHIFT) & HALP_PD_MASK];
    if (!Frame->Present) {
        *TargetLevel = HALP_PD_LEVEL;
        return false;
    } else if (Frame->PageSize) {
        *TargetLevel = HALP_PD_LEVEL;
        *TargetFrame = Frame;
        return true;
    }

    Frame = &HALP_PT_BASE[(VirtualAddress >> HALP_PT_SHIFT) & HALP_PT_MASK];
    *TargetLevel = HALP_PT_LEVEL;
    if (!Frame->Present) {
        return false;
    } else {
        *TargetFrame = Frame;
        return true;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks if the bottom most level that contained a page table entry can be
 *     cleaned up (and repeats the process with the parents if so).
 *
 * PARAMETERS:
 *     VirtualAddress - Virtual address to process.
 *     TargetLevel - Level returned by GetFrame when traversing the table.
 *
 * RETURN VALUE:
 *     true if we cleaned up anything (you should probably do a CR3 reload instead of TLB flush
 *     in this case).
 *-----------------------------------------------------------------------------------------------*/
static bool CleanFrame(uint64_t VirtualAddress, uint64_t TargetLevel) {
    bool Result = false;

    while (TargetLevel > HALP_PML4_LEVEL) {
        TableLevel Parent = TableLevels[TargetLevel - 1];
        TableLevel Current = TableLevels[TargetLevel];
        uint64_t ParentIndex = (VirtualAddress >> Parent.Shift) & Parent.Mask;
        uint64_t CurrentIndex = (VirtualAddress >> Current.Shift) & Current.Mask & ~511;

        /* Check if all entries inside this level are empty. */
        bool IsEmpty = true;
        for (uint64_t Page = 0; Page < 512; Page++) {
            if (Current.Base[CurrentIndex + Page].Present) {
                IsEmpty = false;
                break;
            }
        }

        if (!IsEmpty) {
            break;
        }

        /* If so, free up ourselves in the parent, and keep on going. */
        HalpPageFrame *ParentFrame = &Parent.Base[ParentIndex];

        if (MiPageList) {
            MmFreeSinglePage(ParentFrame->Address << MM_PAGE_SHIFT);
        }

        ParentFrame->RawData = 0;
        TargetLevel--;
        Result = true;
    }

    return Result;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function invalidates a range of pages (or the whole page directory) after something got
 *     unmapped.
 *
 * PARAMETERS:
 *     ContextPointer - Information about what to invalidate.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void IpiRoutine(void *ContextPointer) {
    IpiContext *Context = ContextPointer;
    if (Context->ReloadCr3) {
        __asm__ volatile("mov %%cr3, %%rax; mov %%rax, %%cr3;" : : : "%rax", "memory");
    } else {
        for (uint64_t Address = Context->Start; Address < Context->End; Address += HALP_PT_SIZE) {
            __asm__ volatile("invlpg (%0)" : : "r"(Address) : "memory");
        }
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function grabs the physical address of the specified virtual address.
 *
 * PARAMETERS:
 *     VirtualAddress - Address we need the physical page of.
 *
 * RETURN VALUE:
 *     Physical address, or 0 on failure.
 *-----------------------------------------------------------------------------------------------*/
uint64_t HalpGetPhysicalAddress(void *VirtualAddress) {
    uint64_t TargetLevel = 0;
    HalpPageFrame *TargetFrame = NULL;
    if (GetFrame((uint64_t)VirtualAddress, &TargetLevel, &TargetFrame)) {
        return (TargetFrame->Address << HALP_PT_SHIFT) |
               ((uint64_t)VirtualAddress & (TableLevels[TargetLevel].Size - 1));
    } else {
        return 0;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function maps a range of physical addresses into virtual memory.
 *
 * PARAMETERS:
 *     VirtualAddress - Destination address.
 *     PhysicalAddress - Source address.
 *     Size - How many bytes we want to map.
 *     Flags - How we want to map the pages.
 *
 * RETURN VALUE:
 *     true on success, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
bool HalpMapPages(void *VirtualAddress, uint64_t PhysicalAddress, uint64_t Size, int Flags) {
    /* Ensure 4KiB alignment on small pages. */
    if (((uint64_t)VirtualAddress & (HALP_PT_SIZE - 1)) || (PhysicalAddress & (HALP_PT_SIZE - 1)) ||
        (Size & (HALP_PT_SIZE - 1))) {
        return false;
    }

    uint64_t Target = (uint64_t)VirtualAddress;
    uint64_t Source = PhysicalAddress;

    while (Size) {
        /* Extending this to PML5 should be as easy as handling the PML5 level the same as PML4. */
        HalpPageFrame *CurrentFrame = &HALP_PML4_BASE[(Target >> HALP_PML4_SHIFT) & HALP_PML4_MASK];
        HalpPageFrame *NextFrame = &HALP_PDPT_BASE[(Target >> HALP_PDPT_SHIFT) & HALP_PDPT_MASK];
        if (!AllocateFrame(CurrentFrame, NextFrame)) {
            return false;
        }

        /* TODO: support large pages as well. */
        CurrentFrame = NextFrame;
        NextFrame = &HALP_PD_BASE[(Target >> HALP_PD_SHIFT) & HALP_PD_MASK];
        if (!AllocateFrame(CurrentFrame, NextFrame)) {
            return false;
        }

        CurrentFrame = NextFrame;
        NextFrame = &HALP_PT_BASE[(Target >> HALP_PT_SHIFT) & HALP_PT_MASK];
        if (!AllocateFrame(CurrentFrame, NextFrame)) {
            return false;
        }

        /* Maybe we should return failure on already mapped pages instead? */
        CurrentFrame = NextFrame;
        if (!CurrentFrame->Present) {
            BuildFrame(CurrentFrame, Source, HALP_PT_LEVEL, Flags);
        }

        Target += HALP_PT_SIZE;
        Source += HALP_PT_SIZE;
        Size = Size > HALP_PT_SIZE ? Size - HALP_PT_SIZE : 0;
    }

    return true;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function unmaps a range of physical addresses from virtual memory.
 *
 * PARAMETERS:
 *     VirtualAddress - Target address.
 *     Size - Size of the target range.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpUnmapPages(void *VirtualAddress, uint64_t Size) {
    uint64_t Address = (uint64_t)VirtualAddress;
    uint64_t End = Address + Size;
    bool ReloadCr3 = Size >= 32 * HALP_PT_SIZE;

    /* Ensure at least proper 4KiB alignment. */
    if ((Address & (HALP_PT_SIZE - 1)) || (Size & (HALP_PT_SIZE - 1))) {
        return;
    }

    while (Size) {
        uint64_t TargetLevel = 0;
        HalpPageFrame *TargetFrame = NULL;
        bool Present = GetFrame(Address, &TargetLevel, &TargetFrame);
        uint64_t TargetSize = TableLevels[TargetLevel].Size;

        /* We'll be freeing large pages if we match the proper alignment (maybe we shouldn't do it
         * like this?). */
        if (Present && TargetLevel != HALP_PT_LEVEL && !(Address & (TargetSize - 1))) {
            TargetFrame->Present = 0;
            ReloadCr3 = true;
            CleanFrame(Address, TargetLevel);
        } else if (Present && TargetLevel == HALP_PT_LEVEL) {
            TargetFrame->Present = 0;
            ReloadCr3 = ReloadCr3 || CleanFrame(Address, TargetLevel);
        }

        Address += TargetSize;
        Size = Size > TargetSize ? Size - TargetSize : 0;
    }

    IpiContext Context;
    Context.ReloadCr3 = ReloadCr3;
    Context.Start = (uint64_t)VirtualAddress;
    Context.End = End;

    /* No need for a broadcast if we're still too early in the initailization phase. */
    if (HalpSmpInitializationComplete) {
        KeRequestIpiRoutine(IpiRoutine, &Context);
    } else {
        IpiRoutine(&Context);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function maps a range of physical addresses (assumed to be device/MMIO) into contiguous
 *     virtual memory.
 *
 * PARAMETERS:
 *     PhysicalAddress - Source address, does not need to be aligned to MM_PAGE_SIZE.
 *     Size - Size in bytes of the region (also doesn't need to be aligned/a multiple of
 *            MM_PAGE_SIZE).
 *
 * RETURN VALUE:
 *     Pointer to the start of the virtual memory range on success, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
void *MmMapSpace(uint64_t PhysicalAddress, size_t Size) {
    uint64_t Source = PhysicalAddress & ~(HALP_PT_SIZE - 1);
    uint64_t End = (PhysicalAddress + Size + HALP_PT_SIZE - 1) & ~(HALP_PT_SIZE - 1);

    /* Map any unmapped pages as read/write + uncachable; We don't need to allocate any pool space,
     * as we already have a 1-to-1 space in virtual memory for the mappings. */
    if (!HalpMapPages(
            (void *)(MI_VIRTUAL_OFFSET + Source), Source, End - Source, MI_MAP_WRITE | MI_MAP_UC)) {
        return NULL;
    }

    return (void *)(MI_VIRTUAL_OFFSET + PhysicalAddress);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function unmaps physical address previously mapped by MmMapSpace, do not use this on
 *     normal heap pages!
 *
 * PARAMETERS:
 *     VirtualAddress - Value previously returned by MmMapSpace.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void MmUnmapSpace(void *VirtualAddress) {
    (void)VirtualAddress;
}
