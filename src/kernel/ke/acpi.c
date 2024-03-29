/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <ki.h>
#include <mi.h>
#include <string.h>
#include <vid.h>

static uint64_t BaseAddress = 0;
static int TableType = KI_ACPI_NONE;

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
    if (TableType == KI_ACPI_NONE) {
        VidPrint(VID_MESSAGE_ERROR, "Kernel HAL", "the host is not ACPI-compliant\n");
        KeFatalError(KE_BAD_ACPI_TABLES);
    }

    /* DSDT is contained inside the XDsdt (or the Dsdt) field of the FADT; Other than that, just
       do a linear search on the R/XSDT. */

    SdtHeader *Header = MI_PADDR_TO_VADDR(BaseAddress);
    uint32_t *RsdtTables = (uint32_t *)(Header + 1);
    uint64_t *XsdtTables = (uint64_t *)(Header + 1);

    int IsXsdt = TableType == KI_ACPI_XSDT;
    int Occourances = 0;

    /* The DSDT is no guaranteed to be in any of the R/XSDT entries, so we need to grab its
       pointer inside the FADT. */
    if (!memcmp(Signature, "DSDT", 4)) {
        FadtHeader *Fadt = (FadtHeader *)KiFindAcpiTable("FACP", 0);
        if (!Header) {
            return NULL;
        }

        Header = MI_PADDR_TO_VADDR(IsXsdt && Fadt->XDsdt ? Fadt->XDsdt : Fadt->Dsdt);
        if (!Checksum((char *)Header, Header->Length) || memcmp(Header->Signature, "DSDT", 4)) {
            KeFatalError(KE_BAD_ACPI_TABLES);
        }

        return Header;
    }

    if (memcmp(Header->Signature, IsXsdt ? "XSDT" : "RSDT", 4) ||
        !Checksum((char *)Header, Header->Length)) {
        KeFatalError(KE_BAD_ACPI_TABLES);
    }

    for (uint32_t i = 0; i < (Header->Length - sizeof(SdtHeader)) / (IsXsdt ? 8 : 4); i++) {
        SdtHeader *Header = MI_PADDR_TO_VADDR(IsXsdt ? XsdtTables[i] : RsdtTables[i]);

        if (!Checksum((char *)Header, Header->Length)) {
            continue;
        } else if (!memcmp(Header->Signature, Signature, 4) && Occourances++ == Index) {
            return Header;
        }
    }

    return NULL;
}
