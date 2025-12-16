/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/ke.h>
#include <kernel/mm.h>
#include <string.h>

typedef struct {
    char Signature[4];
    uint64_t PhysicalAddress;
    void *VirtualAddress;
    size_t Length;
} CacheEntry;

CacheEntry *Entries = NULL;
CacheEntry *DsdtEntry = NULL;
CacheEntry *FacsEntry = NULL;
int EntryCount = 0;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function calculates and validates the checksum for a system table.
 *
 * PARAMETERS:
 *     Table - Pointer to the start of the table.
 *     Length - Size of the table.
 *
 * RETURN VALUE:
 *     true if the checksum is valid, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
static bool Checksum(const char *Table, uint32_t Length) {
    uint8_t Sum = 0;

    while (Length--) {
        Sum += *(Table++);
    }

    return !Sum;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function caches the entries of the RSDT/XSDT table for later use by the kernel or
 *     drivers.
 *
 * PARAMETERS:
 *     LoaderBlock - Data prepared by the boot loader for us.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpInitializeLateAcpi(KiLoaderBlock *LoaderBlock) {
    /* For now, let's just not initialize anything if we don't have an ACPI table (this is probably
     * going to cause issues when loading the ACPI driver if the architecture/platform needs it,
     * but, we can ignore it for now). */
    if (!LoaderBlock->Acpi.RootTable) {
        return;
    }

    /* The loader has nicely already cached the size of the root SDT, so we don't need to do the
     * trick of mapping it twice. */
    HalpSdtHeader *RootTable = MmMapSpace(
        MM_SPACE_NORMAL, (uintptr_t)LoaderBlock->Acpi.RootTable, LoaderBlock->Acpi.RootTableSize);
    if (!RootTable) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_ACPI_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_BAD_RSDT_TABLE,
            KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
            0);
    }

    /* If the signature is wrong, probably something is VERY wrong; Checksum should always be
     * correct, but consider relaxing this if we end up finding out some firmwares either don't fill
     * the field, or have it set to an incorrect value. */
    int IsXsdt = LoaderBlock->Acpi.Version == 2;
    if (memcmp(RootTable->Signature, IsXsdt ? "XSDT" : "RSDT", 4) ||
        !Checksum((char *)RootTable, RootTable->Length)) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_ACPI_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_BAD_RSDT_TABLE,
            KE_PANIC_PARAMETER_INVALID_TABLE_CHECKSUM,
            0);
    }

    /* Just a linear array to store everything should be fine.  */
    Entries = MmAllocatePool(
        sizeof(CacheEntry) * ((RootTable->Length - sizeof(HalpSdtHeader)) / (IsXsdt ? 8 : 4) + 2),
        MM_POOL_TAG_ACPI);
    if (!Entries) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_ACPI_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_BAD_RSDT_TABLE,
            KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
            0);
    }

    uintptr_t DsdtAddress = 0;
    uintptr_t FacsAddress = 0;
    uint32_t *RsdtTables = (uint32_t *)(RootTable + 1);
    uint64_t *XsdtTables = (uint64_t *)(RootTable + 1);
    for (uint32_t i = 0; i < (RootTable->Length - sizeof(HalpSdtHeader)) / (IsXsdt ? 8 : 4); i++) {
        uintptr_t Address = IsXsdt ? XsdtTables[i] : RsdtTables[i];
        HalpSdtHeader *Header = MmMapSpace(MM_SPACE_NORMAL, Address, MM_PAGE_SIZE);
        if (!Header) {
            KeFatalError(
                KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_ACPI_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_BAD_RSDT_TABLE,
                KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
                0);
        }

        /* Ignore the DSDT (and the FACS), we'll grab it out of the FADT in a bit. */
        if (!memcmp(Header->Signature, "DSDT", 4) || !memcmp(Header->Signature, "FACS", 4)) {
            MmUnmapSpace(Header, MM_PAGE_SIZE);
            continue;
        }

        /* Talking about the FADT, grab the two we taked about out of it (if we find it). */
        if (!memcmp(Header->Signature, "FACP", 4)) {
            HalpFadtHeader *FadtHeader = (HalpFadtHeader *)Header;

            if (!DsdtAddress) {
                DsdtAddress = FadtHeader->XDsdt ? FadtHeader->XDsdt : FadtHeader->Dsdt;
            }

            if (!FacsAddress) {
                FacsAddress = FadtHeader->XFirmwareControl ? FadtHeader->XFirmwareControl
                                                           : FadtHeader->FirmwareCtrl;
            }
        }

        /* Just the first page is enough to grab the signature and size, and then we can store the
         * table already (skip checksum checking until someone actually requests this table be
         * mapped whole). */
        memcpy(Entries[EntryCount].Signature, Header->Signature, 4);
        Entries[EntryCount].PhysicalAddress = Address;
        Entries[EntryCount].VirtualAddress = NULL;
        Entries[EntryCount].Length = Header->Length;
        EntryCount++;
        MmUnmapSpace(Header, MM_PAGE_SIZE);
    }

    /* If we have a DSDT, we can add it to the cache (and cache the entry position). */
    if (DsdtAddress) {
        HalpSdtHeader *Header = MmMapSpace(MM_SPACE_NORMAL, DsdtAddress, MM_PAGE_SIZE);
        if (!Header) {
            KeFatalError(
                KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_ACPI_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_BAD_DSDT_TABLE,
                KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
                0);
        }

        DsdtEntry = &Entries[EntryCount++];
        memcpy(DsdtEntry->Signature, Header->Signature, 4);
        DsdtEntry->PhysicalAddress = DsdtAddress;
        DsdtEntry->VirtualAddress = NULL;
        DsdtEntry->Length = Header->Length;
        MmUnmapSpace(Header, MM_PAGE_SIZE);
    }

    /* Do the same for the FACS as well. */
    if (FacsAddress) {
        HalpSdtHeader *Header = MmMapSpace(MM_SPACE_NORMAL, FacsAddress, MM_PAGE_SIZE);
        if (!Header) {
            KeFatalError(
                KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_ACPI_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_BAD_FACS_TABLE,
                KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
                0);
        }

        FacsEntry = &Entries[EntryCount++];
        memcpy(FacsEntry->Signature, Header->Signature, 4);
        FacsEntry->PhysicalAddress = FacsAddress;
        FacsEntry->VirtualAddress = NULL;
        FacsEntry->Length = Header->Length;
        MmUnmapSpace(Header, MM_PAGE_SIZE);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to search for the given ACPI table.
 *
 * PARAMETERS:
 *     Signature - Signature of the required entry.
 *     Index - Index of the required entry.
 *
 * RETURN VALUE:
 *     Pointer to the found table, or NULL if not found.
 *-----------------------------------------------------------------------------------------------*/
void *HalFindAcpiTable(const char *Signature, int Index) {
    CacheEntry *Entry = NULL;

    /* Shortcut: We already know if we have a DSDT (or a FACS), no need for a search. */
    if (!memcmp(Signature, "DSDT", 4)) {
        Entry = DsdtEntry;
    } else if (!memcmp(Signature, "FACS", 4)) {
        Entry = FacsEntry;
    } else {
        for (int i = 0; i < EntryCount; i++) {
            if (!memcmp(Entries[i].Signature, Signature, 4) && !Index--) {
                Entry = &Entries[i];
                break;
            }
        }
    }

    /* No work left if we already grabbed this table before. */
    if (!Entry) {
        return NULL;
    } else if (Entry->VirtualAddress) {
        return Entry->VirtualAddress;
    }

    /* Our initialization loop already saved the table size, so we just map it. */
    Entry->VirtualAddress = MmMapSpace(MM_SPACE_NORMAL, Entry->PhysicalAddress, Entry->Length);
    if (!Entry->VirtualAddress) {
        return NULL;
    }

    /* And then validate the checksum. */
    if (!Checksum((char *)Entry->VirtualAddress, Entry->Length)) {
        MmUnmapSpace(Entry->VirtualAddress, Entry->Length);
        Entry->VirtualAddress = NULL;
        return NULL;
    }

    return Entry->VirtualAddress;
}
