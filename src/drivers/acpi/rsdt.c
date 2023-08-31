/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpi.h>
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
 *     This function initializes the ACPI subsystem using the RSDT (ACPI 1.0).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpipInitializeFromRsdt() {
    SdtHeader *Rsdt = MI_PADDR_TO_VADDR(KiGetAcpiBaseAddress());
    uint32_t *Tables = (uint32_t *)(Rsdt + 1);

    if (memcmp(Rsdt->Signature, "RSDT", 4) || !Checksum((char *)Rsdt, Rsdt->Length)) {
        KeFatalError(KE_CORRUPTED_HARDWARE_STRUCTURES);
    }

    for (uint32_t i = 0; i < (Rsdt->Length - sizeof(SdtHeader)) / 4; i++) {
        SdtHeader *Header = MI_PADDR_TO_VADDR(Tables[i]);

        if (!Checksum((char *)Header, Header->Length)) {
            KeFatalError(KE_CORRUPTED_HARDWARE_STRUCTURES);
        }

        /* The FADT SHOULD always contain the DSDT pointer, as the DSDT table itself doesn't
           need to be contained in the RSDT. */
        if (!memcmp(Header->Signature, "FACP", 4)) {
            Header = MI_PADDR_TO_VADDR(((FadtHeader *)Header)->Dsdt);

            if (!Checksum((char *)Header, Header->Length) || memcmp(Header->Signature, "DSDT", 4)) {
                KeFatalError(KE_CORRUPTED_HARDWARE_STRUCTURES);
            }

            AcpipPopulateTree((char *)(Header + 1), Header->Length - sizeof(SdtHeader));
        } else if (!memcmp(Header->Signature, "DSDT", 4) || !memcmp(Header->Signature, "SSDT", 4)) {
            AcpipPopulateTree((char *)(Header + 1), Header->Length - sizeof(SdtHeader));
        }
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the ACPI subsystem using the XSDT (ACPI 2.0+).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpipInitializeFromXsdt() {
    SdtHeader *Xsdt = MI_PADDR_TO_VADDR(KiGetAcpiBaseAddress());
    uint64_t *Tables = (uint64_t *)(Xsdt + 1);

    if (memcmp(Xsdt->Signature, "XSDT", 4) || !Checksum((char *)Xsdt, Xsdt->Length)) {
        KeFatalError(KE_CORRUPTED_HARDWARE_STRUCTURES);
    }

    for (uint32_t i = 0; i < (Xsdt->Length - sizeof(SdtHeader)) / 8; i++) {
        SdtHeader *Header = MI_PADDR_TO_VADDR(Tables[i]);

        if (!Checksum((char *)Header, Header->Length)) {
            KeFatalError(KE_CORRUPTED_HARDWARE_STRUCTURES);
        }

        /* The FADT SHOULD always contain the DSDT pointer, as the DSDT table itself doesn't
           need to be contained in the XSDT. */
        if (!memcmp(Header->Signature, "FACP", 4)) {
            Header = MI_PADDR_TO_VADDR(((FadtHeader *)Header)->Dsdt);

            if (!Checksum((char *)Header, Header->Length) || memcmp(Header->Signature, "DSDT", 4)) {
                KeFatalError(KE_CORRUPTED_HARDWARE_STRUCTURES);
            }

            AcpipPopulateTree((char *)(Header + 1), Header->Length - sizeof(SdtHeader));
        } else if (!memcmp(Header->Signature, "DSDT", 4) || !memcmp(Header->Signature, "SSDT", 4)) {
            AcpipPopulateTree((char *)(Header + 1), Header->Length - sizeof(SdtHeader));
        }
    }
}
