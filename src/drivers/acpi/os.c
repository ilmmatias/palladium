/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <crt_impl.h>
#include <ke.h>
#include <mm.h>
#include <stdarg.h>
#include <string.h>
#include <vid.h>

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
            AcpipShowErrorMessage(ACPI_REASON_CORRUPTED_TABLES, "invalid DSDT table\n");
        }

        return Header;
    }

    if (memcmp(Header->Signature, IsXsdt ? "XSDT" : "RSDT", 4) ||
        !Checksum((char *)Header, Header->Length)) {
        AcpipShowErrorMessage(ACPI_REASON_CORRUPTED_TABLES, "invalid R/XSDT table\n");
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

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Wrapper around VidPutChar for __vprint.
 *
 * PARAMETERS:
 *     Buffer - What we need to display.
 *     Size - Size of that data; The data is not required to be NULL terminated, so this need to be
 *            taken into account!
 *     Context - Always NULL for us.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void PutBuffer(const void *buffer, int size, void *context) {
    (void)context;
    for (int i = 0; i < size; i++) {
        VidPutChar(((const char *)buffer)[i]);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function shows a info (basic debug) message to the screen if allowed
 *     (ACPI_ENABLE_INFO set to 1).
 *
 * PARAMETERS:
 *     Message - Format string; Works the same as printf().
 *     ... - Variadic arguments.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpipShowInfoMessage(const char *Format, ...) {
    if (ACPI_ENABLE_INFO) {
        va_list vlist;
        va_start(vlist, Format);

        VidPutString("ACPI Info: ");
        __vprintf(Format, vlist, NULL, PutBuffer);

        va_end(vlist);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function shows a misc debug message to the screen if allowed
 *     (ACPI_ENABLE_DEBUG set to 1).
 *
 * PARAMETERS:
 *     Message - Format string; Works the same as printf().
 *     ... - Variadic arguments.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpipShowDebugMessage(const char *Format, ...) {
    if (ACPI_ENABLE_DEBUG) {
        va_list vlist;
        va_start(vlist, Format);

        VidPutString("ACPI Debug: ");
        __vprintf(Format, vlist, NULL, PutBuffer);

        va_end(vlist);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function shows a tracing (I/O related and device initialization) message to the screen
 *     if allowed (ACPI_ENABLE_TRACE set to 1).
 *
 * PARAMETERS:
 *     Message - Format string; Works the same as printf().
 *     ... - Variadic arguments.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpipShowTraceMessage(const char *Format, ...) {
    if (ACPI_ENABLE_TRACE) {
        va_list vlist;
        va_start(vlist, Format);

        VidPutString("ACPI Trace: ");
        __vprintf(Format, vlist, NULL, PutBuffer);

        va_end(vlist);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function halts the system with the given reason, printing a debug message to the
 *     screen if possible.
 *
 * PARAMETERS:
 *     Reason - What went wrong.
 *     Message - Format string; Works the same as printf().
 *     ... - Variadic arguments.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void AcpipShowErrorMessage(int Reason, const char *Format, ...) {
    va_list vlist;
    va_start(vlist, Format);

    VidPutString("ACPI Error: ");
    __vprintf(Format, vlist, NULL, PutBuffer);

    va_end(vlist);

    KeFatalError(
        Reason == ACPI_REASON_OUT_OF_MEMORY ? KE_EARLY_MEMORY_FAILURE
                                            : KE_CORRUPTED_HARDWARE_STRUCTURES);
}
