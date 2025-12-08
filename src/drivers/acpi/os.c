/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <acpip.h>
#include <kernel/hal.h>
#include <kernel/kd.h>
#include <kernel/ke.h>
#include <kernel/mm.h>

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
    return HalFindAcpiTable(Signature, Index);
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
    return MmAllocatePool(Size, MM_POOL_TAG_ACPI);
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
    return MmAllocatePool(Elements * ElementSize, MM_POOL_TAG_ACPI);
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
    MmFreePool(Block, MM_POOL_TAG_ACPI);
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
    va_start(Arguments, Message);
    KdPrintVariadic(KD_TYPE_INFO, Message, Arguments);
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
    va_start(Arguments, Message);
    KdPrintVariadic(KD_TYPE_DEBUG, Message, Arguments);
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
    va_start(Arguments, Message);
    KdPrintVariadic(KD_TYPE_TRACE, Message, Arguments);
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
    va_start(Arguments, Message);
    KdPrintVariadic(KD_TYPE_ERROR, Message, Arguments);
    va_end(Arguments);
    KeFatalError(
        KE_PANIC_DRIVER_INITIALIZATION_FAILURE,
        KE_PANIC_PARAMETER_ACPI_INITIALIZATION_FAILURE,
        Reason,
        0,
        0);
}
