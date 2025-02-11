/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/ki.h>
#include <kernel/mm.h>
#include <string.h>

static uint64_t BaseAddress = 0;
static int TableType = KI_ACPI_NONE;
static RtSList ListHead = {};
static bool CacheTableDone = false;

typedef struct __attribute__((packed)) {
    char Signature[4];
    uint32_t Length;
    uint8_t Revision;
    uint8_t Checksum;
    char Unused[26];
} SdtHeader;

typedef struct __attribute__((packed)) {
    SdtHeader Header;
    char Unused1[4];
    uint32_t Dsdt;
    char Unused2[96];
    uint64_t XDsdt;
    char Unused3[136];
} FadtHeader;

typedef struct {
    RtSList ListHeader;
    SdtHeader *SdtHeader;
    int Index;
} CacheEntry;

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
 *     This function maps and caches all entries of the R/XSDT (+ the DSDT), pre calculating (and
 *     checking) their checksums.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void CacheTable(void) {
    CacheTableDone = true;

    if (TableType == KI_ACPI_NONE) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_ACPI_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_BAD_RSDT_TABLE,
            KE_PANIC_PARAMETER_TABLE_NOT_FOUND,
            0);
    }

    /* We're forced to guess initially (as we still don't know the size); Assume <1 page, and
     * remap if wrong. */
    SdtHeader *RootSdt = MmMapSpace(BaseAddress, MM_PAGE_SIZE, MM_SPACE_NORMAL);
    if (!RootSdt) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_ACPI_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_BAD_RSDT_TABLE,
            KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
            0);
    }

    if (RootSdt->Length > MM_PAGE_SIZE) {
        uint64_t Length = RootSdt->Length;
        MmUnmapSpace(RootSdt);
        RootSdt = MmMapSpace(BaseAddress, Length, MM_SPACE_NORMAL);
        if (!RootSdt) {
            KeFatalError(
                KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_ACPI_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_BAD_RSDT_TABLE,
                KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
                0);
        }
    }

    uint32_t *RsdtTables = (uint32_t *)(RootSdt + 1);
    uint64_t *XsdtTables = (uint64_t *)(RootSdt + 1);
    bool IsXsdt = TableType == KI_ACPI_XSDT;

    if (memcmp(RootSdt->Signature, IsXsdt ? "XSDT" : "RSDT", 4) ||
        !Checksum((char *)RootSdt, RootSdt->Length)) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_ACPI_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_BAD_RSDT_TABLE,
            KE_PANIC_PARAMETER_INVALID_TABLE_CHECKSUM,
            0);
    }

    for (uint32_t i = 0; i < (RootSdt->Length - sizeof(SdtHeader)) / (IsXsdt ? 8 : 4); i++) {
        uintptr_t Address = IsXsdt ? XsdtTables[i] : RsdtTables[i];
        SdtHeader *Header = MmMapSpace(Address, MM_PAGE_SIZE, MM_SPACE_NORMAL);
        if (!Header) {
            KeFatalError(
                KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_ACPI_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_BAD_RSDT_TABLE,
                KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
                0);
        } else if (!memcmp(Header->Signature, "DSDT", 4)) {
            MmUnmapSpace(Header);
            continue;
        }

        if (Header->Length > MM_PAGE_SIZE) {
            size_t Length = Header->Length;
            MmUnmapSpace(Header);
            Header = MmMapSpace(Address, Length, MM_SPACE_NORMAL);
            if (!Header) {
                KeFatalError(
                    KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
                    KE_PANIC_PARAMETER_ACPI_INITIALIZATION_FAILURE,
                    KE_PANIC_PARAMETER_BAD_RSDT_TABLE,
                    KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
                    0);
            }
        }

        if (!Checksum((char *)Header, Header->Length)) {
            KeFatalError(
                KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_ACPI_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_BAD_RSDT_TABLE,
                KE_PANIC_PARAMETER_INVALID_TABLE_CHECKSUM,
                0);
        }

        /* Calculate the current index for this entry (should realistically be 0 for most
         * tables). */
        int Index = 0;
        RtSList *ListHeader = ListHead.Next;
        while (ListHeader) {
            CacheEntry *Entry = CONTAINING_RECORD(ListHeader, CacheEntry, ListHeader);

            if (!memcmp(Entry->SdtHeader->Signature, Header->Signature, 4)) {
                Index++;
            }

            ListHeader = ListHeader->Next;
        }

        CacheEntry *Entry = MmAllocatePool(sizeof(CacheEntry), MM_POOL_TAG_ACPI);
        if (!Entry) {
            KeFatalError(
                KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_ACPI_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_BAD_RSDT_TABLE,
                KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
                0);
        }

        Entry->SdtHeader = Header;
        Entry->Index = Index;
        RtPushSList(&ListHead, &Entry->ListHeader);
    }

    /* We're still missing the DSDT (we're not trusting any DSDT in the RSDT), grab the FACP, and
     * the use the DSDT from there. */
    FadtHeader *Fadt = KiFindAcpiTable("FACP", 0);
    if (!Fadt) {
        return;
    }

    uint64_t Address = IsXsdt && Fadt->XDsdt ? Fadt->XDsdt : Fadt->Dsdt;
    SdtHeader *Header = MmMapSpace(Address, MM_PAGE_SIZE, MM_SPACE_NORMAL);
    if (!Header) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_ACPI_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_BAD_DSDT_TABLE,
            KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
            0);
    }

    if (Header->Length > MM_PAGE_SIZE) {
        size_t Length = Header->Length;
        MmUnmapSpace(Header);
        Header = MmMapSpace(Address, Length, MM_SPACE_NORMAL);
        if (!Header) {
            KeFatalError(
                KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_ACPI_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_BAD_DSDT_TABLE,
                KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
                0);
        }
    }

    if (!Checksum((char *)Header, Header->Length) || memcmp(Header->Signature, "DSDT", 4)) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_ACPI_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_BAD_DSDT_TABLE,
            KE_PANIC_PARAMETER_INVALID_TABLE_CHECKSUM,
            0);
    }

    CacheEntry *Entry = MmAllocatePool(sizeof(CacheEntry), MM_POOL_TAG_ACPI);
    if (!Entry) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_ACPI_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_BAD_DSDT_TABLE,
            KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
            0);
    }

    Entry->SdtHeader = Header;
    Entry->Index = 0;
    RtPushSList(&ListHead, &Entry->ListHeader);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function saves the ACPI main (RSDT/XSDT) table base address, which the ACPI driver
 *     can access through the KiAcpi* functions.
 *
 * PARAMETERS:
 *     LoaderBlock - Data prepared by the boot loader for us.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KiSaveAcpiData(KiLoaderBlock *LoaderBlock) {
    BaseAddress = (uint64_t)LoaderBlock->AcpiTable;
    TableType = LoaderBlock->AcpiTableVersion;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function searches for a specific table inside the RSDT/XSDT.
 *
 * PARAMETERS:
 *     Signature - Signature of the required entry.
 *
 * RETURN VALUE:
 *     Pointer to the header of the entry, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
void *KiFindAcpiTable(const char Signature[4], int Index) {
    if (!CacheTableDone) {
        CacheTable();
    }

    RtSList *ListHeader = ListHead.Next;
    while (ListHeader) {
        CacheEntry *Entry = CONTAINING_RECORD(ListHeader, CacheEntry, ListHeader);

        if (Entry->Index == Index && !memcmp(Entry->SdtHeader->Signature, Signature, 4)) {
            return Entry->SdtHeader;
        }

        ListHeader = ListHeader->Next;
    }

    return NULL;
}
