/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <ke.h>
#include <mm.h>
#include <string.h>

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
 *     This function searches for a specific table inside the RSDT/XSDT.
 *
 * PARAMETERS:
 *     Signature - Signature of the required entry.
 *
 * RETURN VALUE:
 *     Pointer to the header of the entry, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
SdtHeader *AcpipFindTable(char Signature[4], int Index) {
    /* DSDT is contained inside the XDsdt (or the Dsdt) field of the FADT; Other than that, just
       do a linear search on the R/XSDT. */

    SdtHeader *Header = MI_PADDR_TO_VADDR(KiGetAcpiBaseAddress());
    uint32_t *RsdtTables = (uint32_t *)(Header + 1);
    uint64_t *XsdtTables = (uint64_t *)(Header + 1);

    int IsXsdt = KiGetAcpiTableType() == KI_ACPI_XSDT;
    int Occourances = 0;

    if (!memcmp(Signature, "DSDT", 4)) {
        FadtHeader *Fadt = (FadtHeader *)AcpipFindTable("FACP", 0);
        if (!Header) {
            return NULL;
        }

        Header = MI_PADDR_TO_VADDR(IsXsdt && Fadt->XDsdt ? Fadt->XDsdt : Fadt->Dsdt);
        if (!Checksum((char *)Header, Header->Length) || memcmp(Header->Signature, "DSDT", 4)) {
            KeFatalError(KE_CORRUPTED_HARDWARE_STRUCTURES);
        }

        return Header;
    }

    if (memcmp(Header->Signature, IsXsdt ? "XSDT" : "RSDT", 4) ||
        !Checksum((char *)Header, Header->Length)) {
        KeFatalError(KE_CORRUPTED_HARDWARE_STRUCTURES);
    }

    for (uint32_t i = 0; i < (Header->Length - sizeof(SdtHeader)) / (IsXsdt ? 8 : 4); i++) {
        SdtHeader *Header = MI_PADDR_TO_VADDR(IsXsdt ? XsdtTables[i] : RsdtTables[i]);

        if (!Checksum((char *)Header, Header->Length)) {
            KeFatalError(KE_CORRUPTED_HARDWARE_STRUCTURES);
        } else if (!memcmp(Header->Signature, Signature, 4) && Occourances++ == Index) {
            return Header;
        }
    }

    return NULL;
}
