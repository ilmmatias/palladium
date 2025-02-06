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
 *     This function enables and sets up the given GSI.
 *
 * PARAMETERS:
 *     Gsi - Which interrupt we wish to enable.
 *     TargetVector - Which vector should be raised on the target CPU once this IRQ triggers.
 *     PinPolarity - 0: Active high, 1: Active low. You probably want this set to 0.
 *     TriggerMode - 0: Edge, 1: Level. You probably want this set to 0.
 *     ApicId - Which CPU wants to handle this vector.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void UnmaskIoapicVector(
    uint8_t Gsi,
    uint8_t TargetVector,
    int PinPolarity,
    int TriggerMode,
    uint32_t ApicId) {
    RtSList *ListHeader = IoapicListHead.Next;

    while (ListHeader) {
        HalpIoapicEntry *Entry = CONTAINING_RECORD(ListHeader, HalpIoapicEntry, ListHeader);

        if (Entry->GsiBase <= Gsi && Gsi < Entry->GsiBase + Entry->Size) {
            WriteIoapicRegister(
                Entry,
                HALP_IOAPIC_REDIR_REG_LOW(Gsi - Entry->GsiBase),
                TargetVector | (PinPolarity << 13) | (TriggerMode << 15));
            WriteIoapicRegister(
                Entry, HALP_IOAPIC_REDIR_REG_HIGH(Gsi - Entry->GsiBase), ApicId << 24);
            return;
        }

        ListHeader = ListHeader->Next;
    }
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
                HalpIoapicEntry *Entry = MmAllocatePool(sizeof(HalpIoapicEntry), "Apic");
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
                Entry->VirtualAddress = MmMapSpace(Record->Ioapic.Address, MM_PAGE_SIZE);
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
                    MmAllocatePool(sizeof(HalpIoapicOverrideEntry), "Apic");
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
 *     This function enables the given IRQ, using the override list if required.
 *
 * PARAMETERS:
 *     Irq - Which IRQ we wish to enable.
 *     Vector - Which IDT IRQ vector it should trigger.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpEnableIrq(uint8_t Irq, uint8_t Vector) {
    uint8_t Gsi = Irq;
    RtSList *ListHeader = IoapicOverrideListHead.Next;
    int PinPolarity = 0;
    int TriggerMode = 0;

    while (ListHeader) {
        HalpIoapicOverrideEntry *Entry =
            CONTAINING_RECORD(ListHeader, HalpIoapicOverrideEntry, ListHeader);

        if (Entry->Irq == Irq) {
            Gsi = Entry->Gsi;
            PinPolarity = Entry->PinPolarity;
            TriggerMode = Entry->TriggerMode;
            break;
        }

        ListHeader = ListHeader->Next;
    }

    UnmaskIoapicVector(Gsi, Vector + 32, PinPolarity, TriggerMode, KeGetCurrentProcessor()->ApicId);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function enables the given GSI, without using the override list.
 *
 * PARAMETERS:
 *     Gsi - Which GSI we wish to enable.
 *     Vector - Which IDT IRQ vector it should trigger.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpEnableGsi(uint8_t Gsi, uint8_t Vector) {
    UnmaskIoapicVector(Gsi, Vector + 32, 0, 1, KeGetCurrentProcessor()->ApicId);
}
