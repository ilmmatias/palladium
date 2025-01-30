/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <console.h>
#include <loader.h>
#include <platform.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates the memory descriptor list using the EFI memory map and our
 *     loaded program list data.
 *
 * PARAMETERS:
 *     LoadedPrograms - Header of the loaded programs list.
 *     FrontBuffer - Physical address of the double/frontbuffer.
 *     FrameBufferSize - Size of the framebuffer.
 *     MemoryDescriptorListHead - Output; Pointer to store the descriptor list head.
 *     MemoryDescriptorStack - Output; List to be initialized as the descriptor entry stack.
 *     MemoryMapSize - Output; Size of the runtime services memory map.
 *     MemoryMap - Output; Pointer to be initialized as the runtime services memory map.
 *     DescriptorSize - Output; Size of each entry in the memory map,
 *     DescriptorVersion - Output; Version of the memory map entry.
 *
 * RETURN VALUE:
 *     true on success, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
bool OslpCreateMemoryDescriptors(
    RtDList *LoadedPrograms,
    void *FrontBuffer,
    UINTN FrameBufferSize,
    RtDList **MemoryDescriptorListHead,
    RtDList *MemoryDescriptorStack,
    UINTN *MemoryMapSize,
    EFI_MEMORY_DESCRIPTOR **MemoryMap,
    UINTN *DescriptorSize,
    UINT32 *DescriptorVersion) {
    /* We can start by doing pre-allocating all the memory descriptor data (as we know exactly how
     * many entries we're allowed to have at most). */
    EFI_STATUS Status =
        gBS->AllocatePool(EfiLoaderData, sizeof(RtDList), (VOID **)MemoryDescriptorListHead);
    if (Status != EFI_SUCCESS) {
        OslPrint("Failed to allocate space for the memory descriptor list head.\r\n");
        OslPrint("The boot process cannot continue.\r\n");
        return false;
    }

    OslpMemoryDescriptor *Descriptors = NULL;
    Status =
        gBS->AllocatePool(EfiLoaderData, sizeof(OslpMemoryDescriptor) * 256, (VOID **)&Descriptors);
    if (Status != EFI_SUCCESS) {
        OslPrint("Failed to allocate space for the memory descriptor list items.\r\n");
        OslPrint("The boot process cannot continue.\r\n");
        return false;
    }

    RtInitializeDList(*MemoryDescriptorListHead);
    RtInitializeDList(MemoryDescriptorStack);
    memset(Descriptors, 0, sizeof(OslpMemoryDescriptor) * 256);

    for (int i = 0; i < 256; i++) {
        RtAppendDList(MemoryDescriptorStack, &Descriptors[i].ListHeader);
    }

    /* Let's try to read up the memory map; This may take a few retries to get right (as
     * GetMemoryMap can allocate memory when you call it). */
    UINTN MapKey = 0;

    while (true) {
        Status = gBS->GetMemoryMap(
            MemoryMapSize, *MemoryMap, &MapKey, DescriptorSize, DescriptorVersion);
        if (Status != EFI_BUFFER_TOO_SMALL) {
            break;
        }

        if (*MemoryMap) {
            gBS->FreePool(*MemoryMap);
        }

        Status = gBS->AllocatePool(EfiLoaderData, *MemoryMapSize, (VOID **)MemoryMap);
        if (Status != EFI_SUCCESS) {
            break;
        }
    }

    if (Status != EFI_SUCCESS) {
        OslPrint("Failed to obtain the memory map using gBS->GetMemoryMap().\r\n");
        OslPrint("There might be something wrong with your UEFI firmware.\r\n");
        OslPrint("The boot process cannot continue.\r\n");
        return false;
    }

    /* We cannot ignore the DescriptorSize, as sizeof(EFI_MEMORY_DESCRIPTOR) might not be the same
     * as DescriptorSize (and we need to advance by DescriptorSize each iteration)! */
    for (UINTN Offset = 0; Offset < *MemoryMapSize; Offset += *DescriptorSize) {
        EFI_MEMORY_DESCRIPTOR *Descriptor =
            (EFI_MEMORY_DESCRIPTOR *)((uint64_t)*MemoryMap + Offset);
        uint64_t BasePage = Descriptor->PhysicalStart >> EFI_PAGE_SHIFT;
        uint64_t PageCount = Descriptor->NumberOfPages;

        /* DO NOT APPEND THE 0 PAGE, if this contains it, edit it away! */
        if (!BasePage) {
            BasePage++;
            if (!--PageCount) {
                continue;
            }
        }

        /* We need to convert the EFI_MEMORY_TYPE into a valid Palladium memory descriptor type. */
        uint8_t Type;

        switch (Descriptor->Type) {
            case EfiConventionalMemory:
            case EfiPersistentMemory:
                Type = PAGE_TYPE_FREE;
                break;
            case EfiLoaderCode:
            case EfiLoaderData:
                Type = PAGE_TYPE_OSLOADER_TEMPORARY;
                break;
            case EfiBootServicesCode:
            case EfiBootServicesData:
                Type = PAGE_TYPE_FIRMWARE_TEMPORARY;
                break;
            case EfiRuntimeServicesCode:
            case EfiRuntimeServicesData:
                Type = PAGE_TYPE_FIRMWARE_PERMANENT;
                break;
            default:
                Type = PAGE_TYPE_SYSTEM_RESERVED;
                break;
        }

        if (Descriptor->Attribute & EFI_MEMORY_RUNTIME) {
            Type = PAGE_TYPE_FIRMWARE_PERMANENT;
        }

        if (!OslpUpdateMemoryDescriptors(
                *MemoryDescriptorListHead, MemoryDescriptorStack, Type, BasePage, PageCount)) {
            return false;
        }
    }

    /* Cut into the free regions with the regions we loaded our kernel and boot driver images. */
    for (RtDList *ListHeader = LoadedPrograms->Next; ListHeader != LoadedPrograms;
         ListHeader = ListHeader->Next) {
        OslpLoadedProgram *Program = CONTAINING_RECORD(ListHeader, OslpLoadedProgram, ListHeader);
        if (!OslpUpdateMemoryDescriptors(
                *MemoryDescriptorListHead,
                MemoryDescriptorStack,
                PAGE_TYPE_LOADED_PROGRAM,
                (uint64_t)Program->PhysicalAddress >> EFI_PAGE_SHIFT,
                (Program->ImageSize + EFI_PAGE_SIZE - 1) >> EFI_PAGE_SHIFT)) {
            return false;
        }
    }

    /* And cut into the free regions with the frontbuffer as well. */
    if (!OslpUpdateMemoryDescriptors(
            *MemoryDescriptorListHead,
            MemoryDescriptorStack,
            PAGE_TYPE_GRAPHICS_BUFFER,
            (uint64_t)FrontBuffer >> EFI_PAGE_SHIFT,
            (FrameBufferSize + EFI_PAGE_SIZE - 1) >> EFI_PAGE_SHIFT)) {
        return false;
    }

    return true;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries inserting or updating data in the memory descriptor list.
 *
 * PARAMETERS:
 *     MemoryDescriptorListHead - Head of the memory descriptor list.
 *     MemoryDescriptorStack - Head of the descriptor entry stack.
 *     Type - Type of the descriptor.
 *     BasePage - First physical page of the region.
 *     PageCount - How many pages this region has.
 *
 * RETURN VALUE:
 *     true on success, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
bool OslpUpdateMemoryDescriptors(
    RtDList *MemoryDescriptorListHead,
    RtDList *MemoryDescriptorStack,
    uint8_t Type,
    uint64_t BasePage,
    uint64_t PageCount) {
    OslpMemoryDescriptor *OtherEntry = NULL;
    OslpMemoryDescriptor *Entry = NULL;
    RtDList *ListHeader = NULL;

    /* First, let's check if we overlap any entries with different types, we need to split/overwrite
     * in that case. */
    for (ListHeader = MemoryDescriptorListHead->Next; ListHeader != MemoryDescriptorListHead;) {
        Entry = CONTAINING_RECORD(ListHeader, OslpMemoryDescriptor, ListHeader);

        if (Entry->Type == Type) {
            /* Just make sure that this isn't a middle overlap (or strict full, not extending beyond
             * the entry borders) of the same type, we can just return in those cases. */
            if (BasePage >= Entry->BasePage &&
                BasePage + PageCount <= Entry->BasePage + Entry->PageCount) {
                return true;
            }

            ListHeader = ListHeader->Next;
            continue;
        }

        /* Full overlap. */
        if (BasePage <= Entry->BasePage &&
            BasePage + PageCount >= Entry->BasePage + Entry->PageCount) {
            Entry->Type = Type;
            Entry->BasePage = BasePage;
            Entry->PageCount = PageCount;
            break;
        }

        /* Overlaps to the left, but not fully to the right. */
        if (BasePage <= Entry->BasePage && BasePage + PageCount > Entry->BasePage &&
            BasePage + PageCount < Entry->BasePage + Entry->PageCount) {
            RtDList *LeftArea = RtPopDList(MemoryDescriptorStack);
            if (LeftArea == MemoryDescriptorStack) {
                OslPrint("Failed to fit all memory map entries into the available 256 slots.\r\n");
                OslPrint("The boot process cannot continue.\r\n");
                return false;
            }

            OslpMemoryDescriptor *LeftEntry =
                CONTAINING_RECORD(LeftArea, OslpMemoryDescriptor, ListHeader);
            LeftEntry->Type = Entry->Type;
            LeftEntry->BasePage = BasePage + PageCount;
            LeftEntry->PageCount = Entry->BasePage + Entry->PageCount - LeftEntry->BasePage;
            RtPushDList(ListHeader, &LeftEntry->ListHeader);

            Entry->Type = Type;
            Entry->BasePage = BasePage;
            Entry->PageCount = PageCount;
            break;
        }

        /* Overlaps to the right, but not fully to the left. */
        if (BasePage > Entry->BasePage && BasePage < Entry->BasePage + Entry->PageCount &&
            BasePage + PageCount >= Entry->BasePage + Entry->PageCount) {
            RtDList *RightArea = RtPopDList(MemoryDescriptorStack);
            if (RightArea == MemoryDescriptorStack) {
                OslPrint("Failed to fit all memory map entries into the available 256 slots.\r\n");
                OslPrint("The boot process cannot continue.\r\n");
                return false;
            }

            OslpMemoryDescriptor *RightEntry =
                CONTAINING_RECORD(RightArea, OslpMemoryDescriptor, ListHeader);
            RightEntry->Type = Entry->Type;
            RightEntry->BasePage = Entry->BasePage;
            RightEntry->PageCount = BasePage - Entry->BasePage;
            RtAppendDList(ListHeader, &RightEntry->ListHeader);

            Entry->Type = Type;
            Entry->BasePage = BasePage;
            Entry->PageCount = PageCount;
            break;
        }

        /* Overlaps in the middle. */
        if (BasePage > Entry->BasePage &&
            BasePage + PageCount < Entry->BasePage + Entry->PageCount) {
            RtDList *LeftArea = RtPopDList(MemoryDescriptorStack);
            if (LeftArea == MemoryDescriptorStack) {
                OslPrint("Failed to fit all memory map entries into the available 256 slots.\r\n");
                OslPrint("The boot process cannot continue.\r\n");
                return false;
            }

            RtDList *RightArea = RtPopDList(MemoryDescriptorStack);
            if (RightArea == MemoryDescriptorStack) {
                OslPrint("Failed to fit all memory map entries into the available 256 slots.\r\n");
                OslPrint("The boot process cannot continue.\r\n");
                return false;
            }

            OslpMemoryDescriptor *LeftEntry =
                CONTAINING_RECORD(LeftArea, OslpMemoryDescriptor, ListHeader);
            LeftEntry->Type = Entry->Type;
            LeftEntry->BasePage = Entry->BasePage;
            LeftEntry->PageCount = BasePage - Entry->BasePage;
            RtPushDList(ListHeader, &LeftEntry->ListHeader);

            OslpMemoryDescriptor *RightEntry =
                CONTAINING_RECORD(RightArea, OslpMemoryDescriptor, ListHeader);
            RightEntry->Type = Entry->Type;
            RightEntry->BasePage = BasePage + PageCount;
            RightEntry->PageCount = Entry->BasePage + Entry->PageCount - RightEntry->BasePage;
            RtAppendDList(ListHeader, &RightEntry->ListHeader);

            Entry->Type = Type;
            Entry->BasePage = BasePage;
            Entry->PageCount = PageCount;

            /* In this case we're just done, we know exactly what is around us (and it is not of the
             * same type), so there's no need to keep on going. */
            return true;
        }

        ListHeader = ListHeader->Next;
    }

    /* If we didn't overlap any other entry, let's attempt to search for a matching region that we
     * can just extend. */
    if (ListHeader == MemoryDescriptorListHead) {
        for (ListHeader = MemoryDescriptorListHead->Next; ListHeader != MemoryDescriptorListHead;) {
            Entry = CONTAINING_RECORD(ListHeader, OslpMemoryDescriptor, ListHeader);

            if (Entry->Type != Type) {
                ListHeader = ListHeader->Next;
                continue;
            } else if (Entry->BasePage + Entry->PageCount == BasePage) {
                Entry->PageCount += PageCount;
                break;
            } else if (BasePage + PageCount == Entry->BasePage) {
                Entry->BasePage = BasePage;
                Entry->PageCount += PageCount;
                break;
            }

            ListHeader = ListHeader->Next;
        }
    }

    /* If we messed with anything, then we can go ahead and try merging the entries
     * around us (before returning). */
    if (ListHeader != MemoryDescriptorListHead) {
        /* Merge backwards. */
        while (true) {
            OtherEntry = CONTAINING_RECORD(ListHeader->Prev, OslpMemoryDescriptor, ListHeader);
            if (OtherEntry->Type == Entry->Type &&
                OtherEntry->BasePage + OtherEntry->PageCount == Entry->BasePage) {
                Entry->BasePage = OtherEntry->BasePage;
                Entry->PageCount += OtherEntry->PageCount;
                RtUnlinkDList(ListHeader->Prev);
            } else {
                break;
            }
        }

        /* Merge forwards. */
        while (true) {
            OtherEntry = CONTAINING_RECORD(ListHeader->Next, OslpMemoryDescriptor, ListHeader);
            if (Entry->Type == OtherEntry->Type &&
                Entry->BasePage + Entry->PageCount == OtherEntry->BasePage) {
                Entry->PageCount += OtherEntry->PageCount;
                RtUnlinkDList(ListHeader->Next);
            } else {
                break;
            }
        }

        return true;
    }

    /* At last, if we failed to find an entry to mess with, we can just allocate a new descriptor
     * and add it to the list. */
    ListHeader = RtPopDList(MemoryDescriptorStack);
    if (ListHeader == MemoryDescriptorStack) {
        OslPrint("Failed to fit all memory map entries into the available 256 slots.\r\n");
        OslPrint("The boot process cannot continue.\r\n");
        return false;
    }

    OslpMemoryDescriptor *Descriptor =
        CONTAINING_RECORD(ListHeader, OslpMemoryDescriptor, ListHeader);
    Descriptor->Type = Type;
    Descriptor->BasePage = BasePage;
    Descriptor->PageCount = PageCount;

    for (ListHeader = MemoryDescriptorListHead->Next; ListHeader != MemoryDescriptorListHead;
         ListHeader = ListHeader->Next) {
        OslpMemoryDescriptor *Entry =
            CONTAINING_RECORD(ListHeader, OslpMemoryDescriptor, ListHeader);
        if (Entry->BasePage + Entry->PageCount > BasePage + PageCount) {
            break;
        }
    }

    RtAppendDList(ListHeader, &Descriptor->ListHeader);
    return true;
}
