/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <amd64/halp.h>

extern void HalpFlushGdt(void);

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes an entry inside the GDT.
 *
 * PARAMETERS:
 *     Processor - Pointer to the processor-specific structure.
 *     EntryOffset - Which entry to initialize.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void InitializeEntry(
    KeProcessor *Processor,
    uint8_t EntryOffset,
    uint64_t Base,
    uint32_t Limit,
    uint8_t Type,
    uint8_t Dpl) {
    HalpGdtEntry *Entry = (HalpGdtEntry *)(Processor->GdtEntries + EntryOffset);
    Entry->LimitLow = Limit & 0xFFFF;
    Entry->BaseLow = Base & 0xFFFFFF;
    Entry->Type = Type;
    Entry->Dpl = Dpl;
    Entry->Present = 1;
    Entry->LimitHigh = (Limit >> 16) & 0x0F;
    Entry->System = 0;
    Entry->LongMode = 1;
    Entry->DefaultBig = 0;
    Entry->Granularity = 1;
    Entry->BaseHigh = (Base >> 24) & 0xFF;
    Entry->BaseUpper = (Base >> 32) & 0xFFFFFFFF;
    Entry->MustBeZero = 0;
}

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
void HalpInitializeGdt(KeProcessor *Processor) {
    /* The NULL descriptor should have already been zeroed out during the processor block
     * allocation. */
    InitializeEntry(Processor, GDT_ENTRY_KCODE, 0, 0xFFFFF, GDT_TYPE_CODE, GDT_DPL_KERNEL);
    InitializeEntry(Processor, GDT_ENTRY_KDATA, 0, 0xFFFFF, GDT_TYPE_DATA, GDT_DPL_KERNEL);
    InitializeEntry(Processor, GDT_ENTRY_UCODE, 0, 0xFFFFF, GDT_TYPE_CODE, GDT_DPL_USER);
    InitializeEntry(Processor, GDT_ENTRY_UDATA, 0, 0xFFFFF, GDT_TYPE_DATA, GDT_DPL_USER);

    uint64_t TssBase = (uint64_t)&Processor->TssEntry;
    uint16_t TssSize = sizeof(HalpTssEntry);
    InitializeEntry(Processor, GDT_ENTRY_TSS, TssBase, TssSize, GDT_TYPE_TSS, GDT_DPL_KERNEL);
    Processor->TssEntry.Rsp0 = (uint64_t)&Processor->SystemStack;
    Processor->TssEntry.IoMapBase = TssSize;

    HalpGdtDescriptor Descriptor;
    Descriptor.Limit = sizeof(Processor->GdtEntries) - 1;
    Descriptor.Base = (uint64_t)Processor->GdtEntries;
    __asm__ volatile("lgdt %0" : : "m"(Descriptor));
    HalpFlushGdt();
    __asm__ volatile("mov $0x28, %%ax; ltr %%ax" : : : "%rax");
}
