/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <ki.h>
#include <mi.h>
#include <string.h>
#include <vid.h>

static uint64_t BaseAddress = 0;
static int TableType = KI_ACPI_NONE;
static RtSList ListHead = {};
static int CacheTableDone = 0;

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
 *     1 if the checksum is valid, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
static int Checksum(const char *Table, uint32_t Length) {
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
    CacheTableDone = 1;

    if (TableType == KI_ACPI_NONE) {
        VidPrint(VID_MESSAGE_ERROR, "Kernel HAL", "the host is not ACPI-compliant\n");
        KeFatalError(KE_PANIC_BAD_SYSTEM_TABLE);
    }

    /* We're forced to guess initially (as we still don't know the size); Assume <1 page, and
     * remap if wrong. */
    SdtHeader *RootSdt = MmMapSpace(BaseAddress, MM_PAGE_SIZE);
    if (!RootSdt) {
        VidPrint(VID_MESSAGE_ERROR, "Kernel HAL", "couldn't map the R/XSDT\n");
        KeFatalError(KE_PANIC_INSTALL_MORE_MEMORY);
    }

    if (RootSdt->Length > MM_PAGE_SIZE) {
        MmUnmapSpace(RootSdt);
        RootSdt = MmMapSpace(BaseAddress, RootSdt->Length);
        if (!RootSdt) {
            VidPrint(VID_MESSAGE_ERROR, "Kernel HAL", "couldn't map the R/XSDT\n");
            KeFatalError(KE_PANIC_INSTALL_MORE_MEMORY);
        }
    }

    uint32_t *RsdtTables = (uint32_t *)(RootSdt + 1);
    uint64_t *XsdtTables = (uint64_t *)(RootSdt + 1);
    int IsXsdt = TableType == KI_ACPI_XSDT;

    if (memcmp(RootSdt->Signature, IsXsdt ? "XSDT" : "RSDT", 4) ||
        !Checksum((char *)RootSdt, RootSdt->Length)) {
        VidPrint(VID_MESSAGE_ERROR, "Kernel HAL", "bad checksum or signature on the RSDT/XSDT\n");
        KeFatalError(KE_PANIC_BAD_SYSTEM_TABLE);
    }

    for (uint32_t i = 0; i < (RootSdt->Length - sizeof(SdtHeader)) / (IsXsdt ? 8 : 4); i++) {
        uintptr_t Address = IsXsdt ? XsdtTables[i] : RsdtTables[i];
        SdtHeader *Header = MmMapSpace(Address, MM_PAGE_SIZE);
        if (!Header) {
            VidPrint(VID_MESSAGE_ERROR, "Kernel HAL", "couldn't map an ACPI table\n");
            KeFatalError(KE_PANIC_INSTALL_MORE_MEMORY);
        } else if (!memcmp(Header->Signature, "DSDT", 4)) {
            MmUnmapSpace(Header);
            continue;
        }

        if (Header->Length > MM_PAGE_SIZE) {
            size_t Length = Header->Length;
            MmUnmapSpace(Header);
            Header = MmMapSpace(Address, Length);
            if (!Header) {
                VidPrint(VID_MESSAGE_ERROR, "Kernel HAL", "couldn't map an ACPI table\n");
                KeFatalError(KE_PANIC_INSTALL_MORE_MEMORY);
            }
        }

        if (!Checksum((char *)Header, Header->Length)) {
            VidPrint(VID_MESSAGE_ERROR, "Kernel HAL", "bad checksum on an ACPI table\n");
            KeFatalError(KE_PANIC_BAD_SYSTEM_TABLE);
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

        CacheEntry *Entry = MmAllocatePool(sizeof(CacheEntry), "KAcp");
        if (!Entry) {
            VidPrint(
                VID_MESSAGE_ERROR,
                "Kernel HAL",
                "couldn't allocate memory for an ACPI table cache entry\n");
            KeFatalError(KE_PANIC_INSTALL_MORE_MEMORY);
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
    SdtHeader *Header = MmMapSpace(Address, MM_PAGE_SIZE);
    if (!Header) {
        VidPrint(VID_MESSAGE_ERROR, "Kernel HAL", "couldn't map the DSDT\n");
        KeFatalError(KE_PANIC_INSTALL_MORE_MEMORY);
    }

    if (Header->Length > MM_PAGE_SIZE) {
        size_t Length = Header->Length;
        MmUnmapSpace(Header);
        Header = MmMapSpace(Address, Length);
        if (!Header) {
            VidPrint(VID_MESSAGE_ERROR, "Kernel HAL", "couldn't map the DSDT\n");
            KeFatalError(KE_PANIC_INSTALL_MORE_MEMORY);
        }
    }

    if (!Checksum((char *)Header, Header->Length) || memcmp(Header->Signature, "DSDT", 4)) {
        VidPrint(VID_MESSAGE_ERROR, "Kernel HAL", "bad checksum or signature on the DSDT\n");
        KeFatalError(KE_PANIC_BAD_SYSTEM_TABLE);
    }

    CacheEntry *Entry = MmAllocatePool(sizeof(CacheEntry), "KAcp");
    if (!Entry) {
        VidPrint(
            VID_MESSAGE_ERROR,
            "Kernel HAL",
            "couldn't allocate memory for the cache entry for the DSDT\n");
        KeFatalError(KE_PANIC_INSTALL_MORE_MEMORY);
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
