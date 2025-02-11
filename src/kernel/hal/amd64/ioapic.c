/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/ke.h>
#include <kernel/mm.h>
#include <kernel/vid.h>

static RtSList IoapicListHead = {};
static RtSList IoapicOverrideListHead = {};

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reads the given IOAPIC register.
 *
 * PARAMETERS:
 *     Entry - Header containing the IOAPIC entry info.
 *     Number - Which register we want to read.
 *
 * RETURN VALUE:
 *     What we've read.
 *-----------------------------------------------------------------------------------------------*/
static uint32_t ReadIoapicRegister(HalpIoapicEntry *Entry, uint8_t Number) {
    *(volatile uint32_t *)(Entry->VirtualAddress + HALP_IOAPIC_INDEX) = Number;
    return *(volatile uint32_t *)(Entry->VirtualAddress + HALP_IOAPIC_DATA);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function writes data into the given IOAPIC register.
 *
 * PARAMETERS:
 *     Entry - Header containing the IOAPIC entry info.
 *     Number - Which register we want to write into.
 *     Data - What we want to write.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void WriteIoapicRegister(HalpIoapicEntry *Entry, uint8_t Number, uint32_t Data) {
    *(volatile uint32_t *)(Entry->VirtualAddress + HALP_IOAPIC_INDEX) = Number;
    *(volatile uint32_t *)(Entry->VirtualAddress + HALP_IOAPIC_DATA) = Data;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function parses the APIC/MADT table, collecting all IOAPICs in the system.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpInitializeIoapic(void) {
    HalpMadtHeader *Madt = KiFindAcpiTable("APIC", 0);
    if (!Madt) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_IOAPIC_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_TABLE_NOT_FOUND,
            0,
            0);
    }

    char *Position = (char *)(Madt + 1);
    while (Position < (char *)Madt + Madt->Length) {
        HalpMadtRecord *Record = (HalpMadtRecord *)Position;

        switch (Record->Type) {
            case HALP_IOAPIC_RECORD: {
                HalpIoapicEntry *Entry = MmAllocatePool(sizeof(HalpIoapicEntry), MM_POOL_TAG_APIC);
                if (!Entry) {
                    KeFatalError(
                        KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
                        KE_PANIC_PARAMETER_IOAPIC_INITIALIZATION_FAILURE,
                        KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
                        0,
                        0);
                }

                Entry->Id = Record->Ioapic.IoapicId;
                Entry->GsiBase = Record->Ioapic.GsiBase;
                Entry->VirtualAddress =
                    MmMapSpace(Record->Ioapic.Address, MM_PAGE_SIZE, MM_SPACE_IO);
                if (!Entry->VirtualAddress) {
                    KeFatalError(
                        KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
                        KE_PANIC_PARAMETER_IOAPIC_INITIALIZATION_FAILURE,
                        KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
                        0,
                        0);
                }

                /* Set some sane defaults for all IOAPICs we find. */
                Entry->Size = (ReadIoapicRegister(Entry, HALP_IOAPIC_VER_REG) >> 16) + 1;
                for (uint8_t i = 0; i < Entry->Size; i++) {
                    WriteIoapicRegister(Entry, HALP_IOAPIC_REDIR_REG_LOW(i), 0x10000);
                    WriteIoapicRegister(Entry, HALP_IOAPIC_REDIR_REG_HIGH(i), 0);
                }

                RtPushSList(&IoapicListHead, &Entry->ListHeader);
                VidPrint(
                    VID_MESSAGE_DEBUG,
                    "Kernel HAL",
                    "found IOAPIC %hhu (GSI base %u, size %u)\n",
                    Entry->Id,
                    Entry->GsiBase,
                    Entry->Size);

                break;
            }

            /* Legacy IRQ -> GSI mappings; We need this to handle any legacy device (such as a
               PIT). */
            case HALP_IOAPIC_SOURCE_OVERRIDE_RECORD: {
                HalpIoapicOverrideEntry *Entry =
                    MmAllocatePool(sizeof(HalpIoapicOverrideEntry), MM_POOL_TAG_APIC);
                if (!Entry) {
                    KeFatalError(
                        KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
                        KE_PANIC_PARAMETER_IOAPIC_INITIALIZATION_FAILURE,
                        KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
                        0,
                        0);
                }

                Entry->Irq = Record->IoapicSourceOverride.IrqSource;
                Entry->Gsi = Record->IoapicSourceOverride.Gsi;
                Entry->PinPolarity = (Record->IoapicSourceOverride.Flags & 2) != 0;
                Entry->TriggerMode = (Record->IoapicSourceOverride.Flags & 8) != 0;
                RtPushSList(&IoapicOverrideListHead, &Entry->ListHeader);
                VidPrint(
                    VID_MESSAGE_DEBUG,
                    "Kernel HAL",
                    "added IOAPIC redir (IRQ %hhu, GSI %hhu) to the list\n",
                    Entry->Irq,
                    Entry->Gsi);

                break;
            }
        }

        Position += Record->Length;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries translating the IRQ into a GSI using the IOAPIC override list.
 *
 * PARAMETERS:
 *     Irq - Which IRQ we wish to translate.
 *     Gsi - Output; Which GSI should be used for this IRQ.
 *     PinPolarity - Output; Polarity that should be used for this IRQ.
 *     TriggerMode - Output; Trigger mode that should be used for this IRQ.
 *
 * RETURN VALUE:
 *     true if there was an IOAPIC override, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
bool HalpTranslateIrq(uint8_t Irq, uint8_t *Gsi, uint8_t *PinPolarity, uint8_t *TriggerMode) {
    for (RtSList *ListHeader = IoapicOverrideListHead.Next; ListHeader;
         ListHeader = ListHeader->Next) {
        HalpIoapicOverrideEntry *Entry =
            CONTAINING_RECORD(ListHeader, HalpIoapicOverrideEntry, ListHeader);
        if (Entry->Irq == Irq) {
            *Gsi = Entry->Gsi;
            *PinPolarity = Entry->PinPolarity;
            *TriggerMode = Entry->TriggerMode;
            return true;
        }
    }

    return false;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function enables the given GSI in the IOAPIC, redirecting it to the given IDT vector.
 *
 * PARAMETERS:
 *     Gsi - Which GSI we wish to enable.
 *     Vector - Which IDT vector it should trigger.
 *     PinPolarity - Polarity that should be used for this GSI.
 *     TriggerMode - Trigger mode that should be used for this GSI.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpEnableGsi(uint8_t Gsi, uint8_t Vector, uint8_t PinPolarity, uint8_t TriggerMode) {
    uint32_t ApicId = KeGetCurrentProcessor()->ApicId;
    RtSList *ListHeader = IoapicListHead.Next;

    while (ListHeader) {
        HalpIoapicEntry *Entry = CONTAINING_RECORD(ListHeader, HalpIoapicEntry, ListHeader);

        if (Entry->GsiBase <= Gsi && Gsi < Entry->GsiBase + Entry->Size) {
            WriteIoapicRegister(
                Entry,
                HALP_IOAPIC_REDIR_REG_LOW(Gsi - Entry->GsiBase),
                Vector | (PinPolarity << 13) | (TriggerMode << 15));
            WriteIoapicRegister(
                Entry, HALP_IOAPIC_REDIR_REG_HIGH(Gsi - Entry->GsiBase), ApicId << 24);
            return;
        }

        ListHeader = ListHeader->Next;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function disables (masks) the given GSI in the IOAPIC.
 *
 * PARAMETERS:
 *     Gsi - Which GSI we wish to disable.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpDisableGsi(uint8_t Gsi) {
    RtSList *ListHeader = IoapicListHead.Next;

    while (ListHeader) {
        HalpIoapicEntry *Entry = CONTAINING_RECORD(ListHeader, HalpIoapicEntry, ListHeader);

        if (Entry->GsiBase <= Gsi && Gsi < Entry->GsiBase + Entry->Size) {
            WriteIoapicRegister(Entry, HALP_IOAPIC_REDIR_REG_LOW(Gsi - Entry->GsiBase), 0x10000);
            WriteIoapicRegister(Entry, HALP_IOAPIC_REDIR_REG_HIGH(Gsi - Entry->GsiBase), 0);
            return;
        }

        ListHeader = ListHeader->Next;
    }
}
