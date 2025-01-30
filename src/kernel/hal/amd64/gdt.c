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
 *     Base - Virtual address of the TSS; Should be zero for all other entries.
 *     Limit - Size of the TSS in bytes; Should 0xFFFFF for all other entries.
 *     Type - Type of the descriptor.
 *     Dpl - Privilege level of the descriptor.
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
    Entry->BaseLow = Base & 0xFFFF;
    Entry->BaseMiddle = (Base >> 16) & 0xFF;
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
    InitializeEntry(Processor, DESCR_SEG_KCODE, 0, 0xFFFFF, GDT_TYPE_CODE, DESCR_DPL_KERNEL);
    InitializeEntry(Processor, DESCR_SEG_KDATA, 0, 0xFFFFF, GDT_TYPE_DATA, DESCR_DPL_KERNEL);
    InitializeEntry(Processor, DESCR_SEG_UCODE, 0, 0xFFFFF, GDT_TYPE_CODE, DESCR_DPL_USER);
    InitializeEntry(Processor, DESCR_SEG_UDATA, 0, 0xFFFFF, GDT_TYPE_DATA, DESCR_DPL_USER);

    uint64_t TssBase = (uint64_t)&Processor->TssEntry;
    uint16_t TssSize = sizeof(HalpTssEntry);
    InitializeEntry(Processor, DESCR_SEG_TSS, TssBase, TssSize, GDT_TYPE_TSS, DESCR_DPL_KERNEL);
    Processor->TssEntry.Rsp0 = (uint64_t)&Processor->SystemStack;
    Processor->TssEntry.Ist[DESCR_IST_NMI - 1] = (uint64_t)&Processor->NmiStack;
    Processor->TssEntry.Ist[DESCR_IST_DOUBLE_FAULT - 1] = (uint64_t)&Processor->DoubleFaultStack;
    Processor->TssEntry.Ist[DESCR_IST_MACHINE_CHECK - 1] = (uint64_t)&Processor->MachineCheckStack;
    Processor->TssEntry.IoMapBase = TssSize;

    HalpGdtDescriptor Descriptor;
    Descriptor.Limit = sizeof(Processor->GdtEntries) - 1;
    Descriptor.Base = (uint64_t)Processor->GdtEntries;
    __asm__ volatile("lgdt %0" : : "m"(Descriptor));
    HalpFlushGdt();
    __asm__ volatile("mov $0x28, %%ax; ltr %%ax" : : : "%rax");
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function updates the TSS Rsp0 using the current thread stack.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpUpdateTss(void) {
    KeProcessor *Processor = KeGetCurrentProcessor();
    Processor->TssEntry.Rsp0 = (uint64_t)Processor->StackBase;
}
