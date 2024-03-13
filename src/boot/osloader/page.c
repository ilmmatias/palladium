/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <console.h>
#include <efi/spec.h>
#include <memory.h>
#include <string.h>

RtDList OslpMemoryDescriptorListHead;
static RtDList OslpFreeMemoryDescriptorListHead;
static OslpMemoryDescriptor OslpMemoryDescriptors[256];

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries merging the given descriptor into an existing memory map entry, merging
 *     neighbours on success.
 *
 * PARAMETERS:
 *     Type - Type of the memory descriptor.
 *     BasePage - First page number.
 *     PageCount - How many pages the entry has.
 *
 * RETURN VALUE:
 *     1 if we don't need to InsertDescriptor(), 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
static int TryMergeDescriptors(uint8_t Type, uint32_t BasePage, uint32_t PageCount) {
    RtDList *ListHeader = OslpMemoryDescriptorListHead.Next;
    OslpMemoryDescriptor *OtherEntry = NULL;
    OslpMemoryDescriptor *Entry = NULL;
    int Success = 0;

    while (ListHeader != &OslpMemoryDescriptorListHead) {
        Entry = CONTAINING_RECORD(ListHeader, OslpMemoryDescriptor, ListHeader);

        if (Entry->Type != Type) {
            ListHeader = ListHeader->Next;
            continue;
        } else if (Entry->BasePage + Entry->PageCount == BasePage) {
            Entry->PageCount += PageCount;
            Success = 1;
            break;
        } else if (BasePage + PageCount == Entry->BasePage) {
            Entry->BasePage = BasePage;
            Entry->PageCount += PageCount;
            Success = 1;
            break;
        }

        ListHeader = ListHeader->Next;
    }

    if (Success) {
        int ContinueMerge = 0;

        do {
            /* Merge forwards. */
            OtherEntry = CONTAINING_RECORD(ListHeader->Next, OslpMemoryDescriptor, ListHeader);
            while (Entry->Type == OtherEntry->Type &&
                   Entry->BasePage + Entry->PageCount == OtherEntry->BasePage) {
                Entry->PageCount += OtherEntry->PageCount;
                RtUnlinkDList(ListHeader->Next);
                OtherEntry = CONTAINING_RECORD(ListHeader->Next, OslpMemoryDescriptor, ListHeader);
                ContinueMerge = 1;
            }

            /* Merge backwards. */
            OtherEntry = CONTAINING_RECORD(ListHeader->Prev, OslpMemoryDescriptor, ListHeader);
            while (OtherEntry->Type == Entry->Type &&
                   OtherEntry->BasePage + OtherEntry->PageCount == Entry->BasePage) {
                Entry->BasePage = OtherEntry->BasePage;
                Entry->PageCount += OtherEntry->PageCount;
                RtUnlinkDList(ListHeader->Prev);
                OtherEntry = CONTAINING_RECORD(ListHeader->Prev, OslpMemoryDescriptor, ListHeader);
                ContinueMerge = 1;
            }

            /* And keep on going until we don't merge anything in either passes. */
        } while (ContinueMerge);
    }

    return Success;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function inserts the given memory descriptor, assuming we DO NOT OVERLAP with anything
 *     else.
 *
 * PARAMETERS:
 *     Descriptor - Which entry we want to insert.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void InsertDescriptor(OslpMemoryDescriptor *Descriptor) {
    RtDList *ListHeader = OslpMemoryDescriptorListHead.Next;

    while (ListHeader != &OslpMemoryDescriptorListHead) {
        OslpMemoryDescriptor *Entry =
            CONTAINING_RECORD(ListHeader, OslpMemoryDescriptor, ListHeader);

        if (Entry->BasePage > Descriptor->BasePage) {
            break;
        }

        ListHeader = ListHeader->Next;
    }

    RtAppendDList(ListHeader, &Descriptor->ListHeader);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function requests the EFI memory map, and initializes our memory map+page allocator.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     EFI_SUCCESS if all went right, whichever error the EFI function returned otherwise.
 *-----------------------------------------------------------------------------------------------*/
EFI_STATUS OslpInitializeMemoryMap(void) {
    UINTN MemoryMapSize = 0;
    EFI_MEMORY_DESCRIPTOR *MemoryMap = NULL;
    UINTN MemoryMapKey = 0;
    UINTN DescriptorSize = 0;
    UINT32 DescriptorVersion = 0;
    EFI_STATUS Status;

    while (1) {
        Status = gBS->GetMemoryMap(
            &MemoryMapSize, MemoryMap, &MemoryMapKey, &DescriptorSize, &DescriptorVersion);
        if (Status != EFI_BUFFER_TOO_SMALL) {
            break;
        }

        /* We'll be allocating some memory, which could make the firmware create some extra
         * memory map entries; As such, overallocate (2 entries, should be enough). */
        MemoryMapSize += DescriptorSize * 2;
        if (MemoryMap) {
            gBS->FreePool(MemoryMap);
        }

        Status = gBS->AllocatePool(EfiLoaderData, MemoryMapSize, (VOID **)&MemoryMap);
        if (Status != EFI_SUCCESS) {
            break;
        }
    }

    if (Status != EFI_SUCCESS) {
        OslPrint("Failed to obtain the memory map using gBS->GetMemoryMap().\r\n");
        OslPrint("There might be something wrong with your UEFI firmware.\r\n");
        OslPrint("The boot process cannot continue.\r\n");
        return Status;
    }

    RtInitializeDList(&OslpMemoryDescriptorListHead);
    RtInitializeDList(&OslpFreeMemoryDescriptorListHead);

    for (size_t i = 0; i < sizeof(OslpMemoryDescriptors) / sizeof(*OslpMemoryDescriptors); i++) {
        RtPushDList(&OslpFreeMemoryDescriptorListHead, &OslpMemoryDescriptors[i].ListHeader);
    }

    for (UINTN i = 0; i < MemoryMapSize; i += DescriptorSize) {
        EFI_MEMORY_DESCRIPTOR *SourceDescriptor = (EFI_MEMORY_DESCRIPTOR *)((UINTN)MemoryMap + i);
        uint32_t BasePage =
            (SourceDescriptor->PhysicalStart + DEFAULT_PAGE_ALLOCATION_GRANULARITY - 1) /
            DEFAULT_PAGE_ALLOCATION_GRANULARITY;
        uint32_t PageCount = ((SourceDescriptor->NumberOfPages << EFI_PAGE_SHIFT) +
                              DEFAULT_PAGE_ALLOCATION_GRANULARITY - 1) /
                             DEFAULT_PAGE_ALLOCATION_GRANULARITY;

        /* DO NOT APPEND THE 0 PAGE, if this contains it, edit it away! */
        if (!BasePage) {
            BasePage++;
            if (!--PageCount) {
                continue;
            }
        }

        /* We use a custom type enum, we need to convert from EFI->whatever we+the kernel can
         * use. */
        uint8_t Type;
        switch (SourceDescriptor->Type) {
            case EfiConventionalMemory:
            case EfiPersistentMemory:
                Type = PAGE_FREE;
                break;
            case EfiLoaderCode:
            case EfiLoaderData:
                Type = PAGE_OSLOADER;
                break;
            case EfiBootServicesCode:
            case EfiBootServicesData:
                Type = PAGE_FIRMWARE_TEMPORARY;
                break;
            case EfiRuntimeServicesCode:
            case EfiRuntimeServicesData:
                Type = PAGE_FIRMWARE_PERMANENT;
                break;
            default:
                Type = PAGE_SYSTEM_RESERVED;
                break;
        }

        /* We really shouldn't be messing with the low 64KiB (mark it as PAGE_LOW_MEMORY). */
        if (Type == PAGE_FREE && BasePage < 16) {
            uint32_t LowPages = 16 - BasePage;

            if (LowPages > PageCount) {
                LowPages = PageCount;
            }

            do {
                if (TryMergeDescriptors(PAGE_LOW_MEMORY, BasePage, LowPages)) {
                    break;
                }

                RtDList *ListHeader = RtPopDList(&OslpFreeMemoryDescriptorListHead);
                if (ListHeader == &OslpFreeMemoryDescriptorListHead) {
                    OslPrint("Failed to append all memory map entries into the kernel list.\r\n");
                    OslPrint("There are over 256 entries, some will be missing.\r\n");
                    OslPrint(
                        "The kernel might not boot properly, or some memory might be missing.\r\n");
                    return Status;
                }

                OslpMemoryDescriptor *TargetDescriptor =
                    CONTAINING_RECORD(ListHeader, OslpMemoryDescriptor, ListHeader);
                TargetDescriptor->Type = PAGE_LOW_MEMORY;
                TargetDescriptor->BasePage = BasePage;
                TargetDescriptor->PageCount = LowPages;
                InsertDescriptor(TargetDescriptor);
            } while (false);

            if (LowPages < PageCount) {
                BasePage += LowPages;
                PageCount -= LowPages;
            } else {
                continue;
            }
        }

        /* Make sure we can't extend an already existing descriptor (and if we can, merge it with
         * its neighbours). */
        if (TryMergeDescriptors(Type, BasePage, PageCount)) {
            continue;
        }

        RtDList *ListHeader = RtPopDList(&OslpFreeMemoryDescriptorListHead);
        if (ListHeader == &OslpFreeMemoryDescriptorListHead) {
            OslPrint("Failed to append all memory map entries into the kernel list.\r\n");
            OslPrint("There are over 256 entries, some will be missing.\r\n");
            OslPrint("The kernel might not boot properly, or some memory might be missing.\r\n");
            break;
        }

        OslpMemoryDescriptor *TargetDescriptor =
            CONTAINING_RECORD(ListHeader, OslpMemoryDescriptor, ListHeader);
        TargetDescriptor->Type = Type;
        TargetDescriptor->BasePage = BasePage;
        TargetDescriptor->PageCount = PageCount;
        InsertDescriptor(TargetDescriptor);
    }

    return Status;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function allocates physical pages for use by the OS loader.
 *
 * PARAMETERS:
 *     Size - How many bytes to allocate; This will get rounded up based on the page size.
 *     Type - Type of the allocation; This can be either LOADED_PROGRAM, OSLOADER, or
 *            BOOT_PROCESSOR.
 *
 * RETURN VALUE:
 *     Base address of the allocation, or NULL if there is no memory left.
 *-----------------------------------------------------------------------------------------------*/
void *OslAllocatePages(uint64_t Size, uint8_t Type) {
    uint32_t Pages =
        (Size + DEFAULT_PAGE_ALLOCATION_GRANULARITY - 1) / DEFAULT_PAGE_ALLOCATION_GRANULARITY;

    if (Type != PAGE_LOADED_PROGRAM && Type != PAGE_OSLOADER && Type != PAGE_BOOT_PROCESSOR &&
        Type != PAGE_BOOT_TABLES) {
        Type = PAGE_OSLOADER;
    }

    for (RtDList *ListHeader = OslpMemoryDescriptorListHead.Next;
         ListHeader != &OslpMemoryDescriptorListHead;
         ListHeader = ListHeader->Next) {
        OslpMemoryDescriptor *Entry =
            CONTAINING_RECORD(ListHeader, OslpMemoryDescriptor, ListHeader);

        if (Entry->Type != PAGE_FREE || Entry->PageCount < Pages) {
            continue;
        }

        int MergedWithNeighbours = TryMergeDescriptors(Type, Entry->BasePage, Pages);
        void *Result = (void *)(uintptr_t)(Entry->BasePage * DEFAULT_PAGE_ALLOCATION_GRANULARITY);

        if (Entry->PageCount == Pages) {
            if (MergedWithNeighbours) {
                /* Merged+consumed the whole block, remove it from the list. */
                RtUnlinkDList(ListHeader);
            } else {
                /* Consumed the whole block but can't merge, flip the type. */
                Entry->Type = Type;
            }
        } else {
            uint32_t BasePage = Entry->BasePage;

            /* We have some pages left, we need to update the original entry. */
            Entry->BasePage += Pages;
            Entry->PageCount -= Pages;

            if (!MergedWithNeighbours) {
                /* And we didn't merge with the previous entry, insert a new descriptor. */
                RtDList *ListHeader = RtPopDList(&OslpFreeMemoryDescriptorListHead);
                if (ListHeader == &OslpFreeMemoryDescriptorListHead) {
                    break;
                }

                OslpMemoryDescriptor *TargetDescriptor =
                    CONTAINING_RECORD(ListHeader, OslpMemoryDescriptor, ListHeader);
                TargetDescriptor->Type = Type;
                TargetDescriptor->BasePage = BasePage;
                TargetDescriptor->PageCount = Pages;
                InsertDescriptor(TargetDescriptor);
            }
        }

        return Result;
    }

    return NULL;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function frees some physical pages previously allocated using OslAllocatePages.
 *
 * PARAMETERS:
 *     Base - First page returned by OslAllocatePages.
 *     Size - How many bytes we had allocated; This will get rounded up based on the page size.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void OslFreePages(void *Base, uint64_t Size) {
    uint32_t BasePage = (uintptr_t)Base / DEFAULT_PAGE_ALLOCATION_GRANULARITY;
    uint32_t Pages =
        (Size + DEFAULT_PAGE_ALLOCATION_GRANULARITY - 1) / DEFAULT_PAGE_ALLOCATION_GRANULARITY;

    for (RtDList *ListHeader = OslpMemoryDescriptorListHead.Next;
         ListHeader != &OslpMemoryDescriptorListHead;
         ListHeader = ListHeader->Next) {
        OslpMemoryDescriptor *Entry =
            CONTAINING_RECORD(ListHeader, OslpMemoryDescriptor, ListHeader);

        if ((Entry->Type != PAGE_LOADED_PROGRAM && Entry->Type != PAGE_OSLOADER &&
             Entry->Type != PAGE_BOOT_PROCESSOR && Entry->Type != PAGE_BOOT_TABLES) ||
            Entry->BasePage > BasePage || Entry->BasePage + Entry->PageCount < BasePage + Pages) {
            continue;
        }

        int MergedWithNeighbours = TryMergeDescriptors(PAGE_FREE, BasePage, Pages);

        if (Entry->PageCount == Pages) {
            if (MergedWithNeighbours) {
                /* Merged+consumed the whole block, remove it from the list. */
                RtUnlinkDList(ListHeader);
            } else {
                /* Consumed the whole block but can't merge, flip the type. */
                Entry->Type = PAGE_FREE;
            }
        } else if (Entry->BasePage == BasePage) {
            /* Start got consumed, and we still have some pages left, we need to update the original
             * entry. */
            Entry->BasePage += Pages;
            Entry->PageCount -= Pages;
        } else if (Entry->BasePage + Entry->PageCount <= BasePage + Pages) {
            /* End got consumed, and we still have some pages left, we need to update the original
             * entry. */
            Entry->PageCount -= Pages;
        } else {
            /* Middle got consumed, we now have something like this:
             *     | used |      | used |
             *            | free |
             * The left used entry is the one we're currently sitting on, but we'll need to
             * create the right one. */
            uint32_t OriginalBasePage = Entry->BasePage;
            uint32_t OriginalPageCount = Entry->PageCount;
            Entry->PageCount = BasePage - Entry->BasePage;

            RtDList *ListHeader = RtPopDList(&OslpFreeMemoryDescriptorListHead);
            if (ListHeader != &OslpFreeMemoryDescriptorListHead) {
                OslpMemoryDescriptor *TargetDescriptor =
                    CONTAINING_RECORD(ListHeader, OslpMemoryDescriptor, ListHeader);
                TargetDescriptor->Type = Entry->Type;
                TargetDescriptor->BasePage = BasePage + Pages;
                TargetDescriptor->PageCount =
                    OriginalBasePage + OriginalPageCount - TargetDescriptor->BasePage;
                InsertDescriptor(TargetDescriptor);
            }
        }

        if (!MergedWithNeighbours) {
            /* We didn't merge with the previous entry, insert a new descriptor. */
            RtDList *ListHeader = RtPopDList(&OslpFreeMemoryDescriptorListHead);
            if (ListHeader != &OslpFreeMemoryDescriptorListHead) {
                OslpMemoryDescriptor *TargetDescriptor =
                    CONTAINING_RECORD(ListHeader, OslpMemoryDescriptor, ListHeader);
                TargetDescriptor->Type = PAGE_FREE;
                TargetDescriptor->BasePage = BasePage;
                TargetDescriptor->PageCount = Pages;
                InsertDescriptor(TargetDescriptor);
            }
        }

        break;
    }
}
