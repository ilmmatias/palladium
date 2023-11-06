/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <amd64/halp.h>

extern void HalpFlushGdt(void);

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the Global Descriptor Table. This, in combination with
 *     InitializeIdt, means we're safe to unmap the first 2MiB (and map the SMP entry point to it).
 *
 * PARAMETERS:
 *     Processor - Pointer to the processor-specific structure.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpInitializeGdt(HalpProcessor *Processor) {
    Processor->GdtEntries[0] = 0x0000000000000000;
    Processor->GdtEntries[1] = 0x00AF9A000000FFFF;
    Processor->GdtEntries[2] = 0x00AF92000000FFFF;
    Processor->GdtEntries[3] = 0x00AFFA000000FFFF;
    Processor->GdtEntries[4] = 0x00AFF2000000FFFF;

    Processor->GdtDescriptor.Limit = sizeof(Processor->GdtEntries) - 1;
    Processor->GdtDescriptor.Base = (uint64_t)Processor->GdtEntries;

    __asm__ volatile("lgdt %0" :: "m"(Processor->GdtDescriptor));
    HalpFlushGdt();
}
