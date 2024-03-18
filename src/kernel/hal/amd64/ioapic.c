/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <amd64/apic.h>
#include <amd64/halp.h>
#include <mi.h>
#include <vid.h>

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
static uint32_t ReadIoapicRegister(IoapicEntry *Entry, uint8_t Number) {
    *(volatile uint32_t *)(Entry->VirtualAddress + IOAPIC_INDEX) = Number;
    return *(volatile uint32_t *)(Entry->VirtualAddress + IOAPIC_DATA);
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
static void WriteIoapicRegister(IoapicEntry *Entry, uint8_t Number, uint32_t Data) {
    *(volatile uint32_t *)(Entry->VirtualAddress + IOAPIC_INDEX) = Number;
    *(volatile uint32_t *)(Entry->VirtualAddress + IOAPIC_DATA) = Data;
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
        IoapicEntry *Entry = CONTAINING_RECORD(ListHeader, IoapicEntry, ListHeader);

        if (Entry->GsiBase <= Gsi && Gsi < Entry->GsiBase + Entry->Size) {
            WriteIoapicRegister(
                Entry,
                IOAPIC_REDIR_REG_LOW(Gsi - Entry->GsiBase),
                TargetVector | (PinPolarity << 13) | (TriggerMode << 15));
            WriteIoapicRegister(Entry, IOAPIC_REDIR_REG_HIGH(Gsi - Entry->GsiBase), ApicId << 24);
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
    MadtHeader *Madt = KiFindAcpiTable("APIC", 0);
    if (!Madt) {
        VidPrint(VID_MESSAGE_ERROR, "Kernel HAL", "couldn't find the MADT table\n");
        KeFatalError(KE_BAD_ACPI_TABLES);
    }

    char *Position = (char *)(Madt + 1);
    while (Position < (char *)Madt + Madt->Length) {
        MadtRecord *Record = (MadtRecord *)Position;

        switch (Record->Type) {
            case IOAPIC_RECORD: {
                IoapicEntry *Entry = MmAllocatePool(sizeof(IoapicEntry), "Apic");
                if (!Entry) {
                    VidPrint(
                        VID_MESSAGE_ERROR, "Kernel HAL", "couldn't allocate space for an IOAPIC\n");
                    KeFatalError(KE_OUT_OF_MEMORY);
                }

                Entry->Id = Record->Ioapic.IoapicId;
                Entry->GsiBase = Record->Ioapic.GsiBase;
                Entry->VirtualAddress = MI_PADDR_TO_VADDR(Record->Ioapic.Address);
                if (!HalpMapPage(
                        Entry->VirtualAddress,
                        Record->Ioapic.Address,
                        MI_MAP_WRITE | MI_MAP_DEVICE)) {
                    VidPrint(VID_MESSAGE_ERROR, "Kernel HAL", "couldn't allocate map an IOAPIC\n");
                    KeFatalError(KE_OUT_OF_MEMORY);
                }

                Entry->Size = (ReadIoapicRegister(Entry, IOAPIC_VER_REG) >> 16) + 1;

                /* Set some sane defaults for all IOAPICs we find. */
                for (uint8_t i = 0; i < Entry->Size; i++) {
                    WriteIoapicRegister(Entry, IOAPIC_REDIR_REG_LOW(i), 0x10000);
                    WriteIoapicRegister(Entry, IOAPIC_REDIR_REG_HIGH(i), 0);
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
            case IOAPIC_SOURCE_OVERRIDE_RECORD: {
                IoapicOverrideEntry *Entry = MmAllocatePool(sizeof(IoapicOverrideEntry), "Apic");
                if (!Entry) {
                    VidPrint(
                        VID_MESSAGE_ERROR,
                        "Kernel HAL",
                        "couldn't allocate space for an IOAPIC source override\n");
                    KeFatalError(KE_OUT_OF_MEMORY);
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
        IoapicOverrideEntry *Entry = CONTAINING_RECORD(ListHeader, IoapicOverrideEntry, ListHeader);

        if (Entry->Irq == Irq) {
            Gsi = Entry->Gsi;
            PinPolarity = Entry->PinPolarity;
            TriggerMode = Entry->TriggerMode;
            break;
        }

        ListHeader = ListHeader->Next;
    }

    UnmaskIoapicVector(
        Gsi,
        Vector + 32,
        PinPolarity,
        TriggerMode,
        ((KeProcessor *)HalGetCurrentProcessor())->ApicId);
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
    UnmaskIoapicVector(Gsi, Vector + 32, 0, 1, ((KeProcessor *)HalGetCurrentProcessor())->ApicId);
}
