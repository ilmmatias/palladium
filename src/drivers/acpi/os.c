/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <acpip.h>
#include <ke.h>
#include <mm.h>
#include <vid.h>

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
    return KiFindAcpiTable(Signature, Index);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Wrapper around the OS' malloc (or malloc-like) routine.
 *
 * PARAMETERS:
 *     Size - Size in bytes of the block.
 *
 * RETURN VALUE:
 *     Start of the allocated block, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
void *AcpipAllocateBlock(size_t Size) {
    return MmAllocatePool(Size, "Acpi");
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Wrapper around the OS' calloc (or calloc-like) routine.
 *
 * PARAMETERS:
 *     Elements - How many elements we have.
 *     ElementSize - Size in bytes of each element.
 *
 * RETURN VALUE:
 *     Start of the allocated block, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
void *AcpipAllocateZeroBlock(size_t Elements, size_t ElementSize) {
    return MmAllocatePool(Elements * ElementSize, "Acpi");
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Wrapper around the OS' free() routine. Should be able to transparently free anything
 *     allocated by AcpipAllocateBlock/ZeroBlock.
 *
 * PARAMETERS:
 *     Block - Start of the block we're trying to free.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpipFreeBlock(void *Block) {
    MmFreePool(Block, "Acpi");
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function shows a info (basic debug) message to the screen if allowed.
 *
 * PARAMETERS:
 *     Message - Format string; Works the same as printf().
 *     ... - Variadic arguments.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpipShowInfoMessage(const char *Message, ...) {
    va_list Arguments;
    va_start(Arguments, Format);
    VidPrintVariadic(VID_MESSAGE_INFO, "ACPI Driver", Message, Arguments);
    va_end(Arguments);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function shows a misc debug message to the screen if allowed.
 *
 * PARAMETERS:
 *     Message - Format string; Works the same as printf().
 *     ... - Variadic arguments.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpipShowDebugMessage(const char *Message, ...) {
    va_list Arguments;
    va_start(Arguments, Format);
    VidPrintVariadic(VID_MESSAGE_DEBUG, "ACPI Driver", Message, Arguments);
    va_end(Arguments);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function shows a tracing (I/O related and device initialization) message to the screen
 *     if allowed.
 *
 * PARAMETERS:
 *     Message - Format string; Works the same as printf().
 *     ... - Variadic arguments.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpipShowTraceMessage(const char *Message, ...) {
    va_list Arguments;
    va_start(Arguments, Format);
    VidPrintVariadic(VID_MESSAGE_TRACE, "ACPI Driver", Message, Arguments);
    va_end(Arguments);
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
[[noreturn]] void AcpipShowErrorMessage(int Reason, const char *Message, ...) {
    va_list Arguments;

    va_start(Arguments, Format);
    VidPrintVariadic(VID_MESSAGE_ERROR, "ACPI Driver", Message, Arguments);
    va_end(Arguments);

    KeFatalError(Reason == ACPI_REASON_OUT_OF_MEMORY ? KE_OUT_OF_MEMORY : KE_BAD_ACPI_TABLES);
}
