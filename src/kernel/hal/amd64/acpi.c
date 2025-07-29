/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/ke.h>
#include <kernel/mm.h>
#include <string.h>

static int Version = 0;
static HalpSdtHeader *RootTable = NULL;

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
 *     This function maps the RSDT/XSDT table with the intent of later searching for subtables
 *     required for initializing amd64-specific functionality (like the IOAPIC).
 *
 * PARAMETERS:
 *     LoaderBlock - Data prepared by the boot loader for us.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpInitializeEarlyAcpi(KiLoaderBlock *LoaderBlock) {
    /* The loader has nicely already cached the size of the root SDT, so we don't need to do the
     * trick of mapping it twice. */
    Version = LoaderBlock->Acpi.Version;
    RootTable = HalpMapEarlyMemory(
        (uint64_t)LoaderBlock->Acpi.RootTable, LoaderBlock->Acpi.RootTableSize, MI_MAP_WRITE);
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
    if (memcmp(RootTable->Signature, Version == 2 ? "XSDT" : "RSDT", 4) ||
        !Checksum((char *)RootTable, RootTable->Length)) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_ACPI_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_BAD_RSDT_TABLE,
            KE_PANIC_PARAMETER_INVALID_TABLE_CHECKSUM,
            0);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to search for the given table inside the ACPI root table. Just a word
 *     of caution: this isn't the function intended to be accessed by drivers, so we don't care
 *     about properly reading the FADT when searching for the DSDT, nor supporting multiple tables
 *     with the same name.
 *
 * PARAMETERS:
 *     Signature - Signature of the required entry.
 *
 * RETURN VALUE:
 *     Pointer to the found table, or NULL if not found.
 *-----------------------------------------------------------------------------------------------*/
void *HalpFindEarlyAcpiTable(const char *Signature) {
    uint32_t *RsdtTables = (uint32_t *)(RootTable + 1);
    uint64_t *XsdtTables = (uint64_t *)(RootTable + 1);
    int IsXsdt = Version == 2;

    for (uint32_t i = 0; i < (RootTable->Length - sizeof(HalpSdtHeader)) / (IsXsdt ? 8 : 4); i++) {
        uintptr_t Address = IsXsdt ? XsdtTables[i] : RsdtTables[i];
        HalpSdtHeader *Header = HalpMapEarlyMemory(Address, MM_PAGE_SIZE, MI_MAP_WRITE);
        if (!Header) {
            KeFatalError(
                KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_ACPI_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_BAD_RSDT_TABLE,
                KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
                0);
        }

        /* Just the first page is enough to check the signature. */
        if (memcmp(Header->Signature, Signature, 4)) {
            HalpUnmapEarlyMemory(Header, MM_PAGE_SIZE);
            continue;
        }

        /* We do need to remap if we guessed the size wrong though. */
        if (Header->Length > MM_PAGE_SIZE) {
            size_t Length = Header->Length;
            HalpUnmapEarlyMemory(Header, MM_PAGE_SIZE);
            Header = HalpMapEarlyMemory(Address, Length, MI_MAP_WRITE);
            if (!Header) {
                KeFatalError(
                    KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
                    KE_PANIC_PARAMETER_ACPI_INITIALIZATION_FAILURE,
                    KE_PANIC_PARAMETER_BAD_RSDT_TABLE,
                    KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
                    0);
            }
        }

        /* BAD_RSDT_TABLE isn't quite the right error, maybe we should make the caller pass the
         * correct error parameter? */
        if (!Checksum((char *)Header, Header->Length)) {
            KeFatalError(
                KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_ACPI_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_BAD_RSDT_TABLE,
                KE_PANIC_PARAMETER_INVALID_TABLE_CHECKSUM,
                0);
        }

        /* The use of this should be limited enough, that caching isn't required. */
        return Header;
    }

    return NULL;
}
