/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <memory.h>
#include <pe.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function clears out any BSS-like sections in bootmgr.exe (with trailing uninitialized
 *     bytes).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BiZeroRequiredSections(void) {
    uint64_t HeaderOffset = *(uint32_t *)(BI_SELF_IMAGE_BASE + 0x3C);
    PeHeaderLoader *Header = (PeHeaderLoader *)(BI_SELF_IMAGE_BASE + HeaderOffset);
    PeSectionHeader *Sections =
        (PeSectionHeader *)((char *)Header + Header->SizeOfOptionalHeader + 24);

    for (int i = 0; i < Header->NumberOfSections; i++) {
        if (Sections[i].VirtualSize > Sections[i].SizeOfRawData) {
            memset(
                (char *)BI_SELF_IMAGE_BASE + Sections[i].VirtualAddress + Sections[i].SizeOfRawData,
                0,
                Sections[i].VirtualSize - Sections[i].SizeOfRawData);
        }
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reserves any physical memory that bootmgr.exe (or its bootstrap) uses.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BiReserveLoaderSections(void) {
    uint64_t HeaderOffset = *(uint32_t *)(BI_SELF_IMAGE_BASE + 0x3C);
    PeHeaderLoader *Header = (PeHeaderLoader *)(BI_SELF_IMAGE_BASE + HeaderOffset);

    /* First page is reserved; NULL/0 pointers are used to indicate error. */
    BiAddMemoryDescriptor(BM_MD_BOOTMGR, 0, BI_PAGE_SIZE);

    /* Boostrap section isn't required to exist (we can know its size by doing the PE base minus
     * it). */
    if (BI_SELF_IMAGE_BASE - BI_BOOTSTRAP_IMAGE_BASE) {
        BiAddMemoryDescriptor(
            BM_MD_BOOTMGR, BI_BOOTSTRAP_IMAGE_BASE, BI_SELF_IMAGE_BASE - BI_BOOTSTRAP_IMAGE_BASE);
    }

    /* Stack and anything else not included in the PE image (nor the bootstrap). */
    if (BI_RESERVED_SIZE) {
        BiAddMemoryDescriptor(BM_MD_BOOTMGR, BI_RESERVED_BASE, BI_RESERVED_SIZE);
    }

    BiAddMemoryDescriptor(BM_MD_BOOTMGR, BI_SELF_IMAGE_BASE, Header->SizeOfImage);
}