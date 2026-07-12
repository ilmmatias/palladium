/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <acpip.h>
#include <kernel/mm.h>
#include <stddef.h>

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
 *     ElementSize - How big each element is.
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
