/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <amd64/apic.h>
#include <amd64/msr.h>
#include <amd64/port.h>
#include <cpuid.h>
#include <ke.h>
#include <mm.h>
#include <vid.h>

static RtSList LapicListHead = {};
static RtSList IoapicListHead = {};
static RtSList IoapicOverrideListHead = {};
static void *LapicAddress = NULL;
static int X2ApicEnabled = 0;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries searching for the specified APIC id in our processor list.
 *
 * PARAMETERS:
 *     Id - What we want to find.
 *
 * RETURN VALUE:
 *     Pointer to the LapicEntry struct, or NULL if we didn't find it.
 *-----------------------------------------------------------------------------------------------*/
static LapicEntry *GetLapic(uint32_t Id) {
    RtSList *Entry = LapicListHead.Next;

    while (Entry) {
        LapicEntry *Lapic = CONTAINING_RECORD(Entry, LapicEntry, ListHeader);

        if (Lapic->ApicId == Id) {
            return Lapic;
        }

        Entry = Entry->Next;
    }

    return NULL;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reads the given Local APIC register.
 *
 * PARAMETERS:
 *     Number - Which register we want to read.
 *
 * RETURN VALUE:
 *     What we've read.
 *-----------------------------------------------------------------------------------------------*/
[[maybe_unused]] static uint32_t ReadLapicRegister(uint32_t Number) {
    if (X2ApicEnabled) {
        return ReadMsr(0x800 + (Number >> 4));
    } else {
        return *(volatile uint32_t *)((char *)LapicAddress + Number);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function writes data into the given Local APIC register.
 *
 * PARAMETERS:
 *     Number - Which register we want to write into.
 *     Data - What we want to write.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void WriteLapicRegister(uint32_t Number, uint32_t Data) {
    if (X2ApicEnabled) {
        WriteMsr(0x800 + (Number >> 4), Data);
    } else {
        *(volatile uint32_t *)((char *)LapicAddress + Number) = Data;
    }
}

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
 *     This function disables the given GSI.
 *
 * PARAMETERS:
 *     Gsi - Which interrupt we wish to disable.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
[[maybe_unused]] static void MaskIoapicVector(uint8_t Gsi) {
    RtSList *ListEntry = IoapicListHead.Next;

    while (ListEntry) {
        IoapicEntry *Entry = CONTAINING_RECORD(ListEntry, IoapicEntry, ListHeader);

        if (Entry->GsiBase <= Gsi && Gsi < Entry->GsiBase + Entry->MaxRedirEntry) {
            WriteIoapicRegister(Entry, IOAPIC_REDIR_REG_LOW(Gsi - Entry->GsiBase), 0x10000);
            return;
        }

        ListEntry = ListEntry->Next;
    }
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
[[maybe_unused]] static void UnmaskIoapicVector(
    uint8_t Gsi,
    uint8_t TargetVector,
    int PinPolarity,
    int TriggerMode,
    uint8_t ApicId) {
    RtSList *ListEntry = IoapicListHead.Next;

    while (ListEntry) {
        IoapicEntry *Entry = CONTAINING_RECORD(ListEntry, IoapicEntry, ListHeader);

        if (Entry->GsiBase <= Gsi && Gsi < Entry->GsiBase + Entry->MaxRedirEntry) {
            WriteIoapicRegister(
                Entry,
                IOAPIC_REDIR_REG_LOW(Gsi - Entry->GsiBase),
                TargetVector | (PinPolarity << 13) | (TriggerMode << 15));
            WriteIoapicRegister(
                Entry, IOAPIC_REDIR_REG_HIGH(Gsi - Entry->GsiBase), (uint32_t)ApicId << 24);
            return;
        }

        ListEntry = ListEntry->Next;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function parses the APIC/MADT table, and gets the system ready to handle interrupts
 *     (and other processors) using the IOAPIC and the Local APIC.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KiInitializeApic(void) {
    MadtHeader *Madt = KiFindAcpiTable("APIC", 0);
    if (!Madt) {
        VidPrint(KE_MESSAGE_ERROR, "APIC", "couldn't find the MADT table\n");
        KeFatalError(KE_BAD_ACPI_TABLES);
    }

    LapicAddress = MI_PADDR_TO_VADDR(Madt->LapicAddress);

    uint32_t Eax, Ebx, Ecx, Edx;
    __cpuid(1, Eax, Ebx, Ecx, Edx);
    if (Ecx & 0x200000) {
        WriteMsr(0x1B, ReadMsr(0x1B) | 0xC00);
        X2ApicEnabled = 1;
    }

    /* We'll need to iterations like this, one to grab all LAPICs and IOAPICs, and one to setup all
       source redirect entries. */
    char *Position = (char *)(Madt + 1);
    while (Position < (char *)Madt + Madt->Length) {
        MadtRecord *Record = (MadtRecord *)Position;

        switch (Record->Type) {
            case LAPIC_RECORD: {
                /* Prevent a bunch of entries with the same APIC ID (but probably different ACPI
                   IDs) filling our processor list. */
                if (GetLapic(Record->Lapic.ApicId)) {
                    break;
                }

                LapicEntry *Entry = MmAllocatePool(sizeof(LapicEntry), "Apic");
                if (!Entry) {
                    VidPrint(
                        KE_MESSAGE_ERROR, "Kernel APIC", "couldn't allocate space for a LAPIC\n");
                    KeFatalError(KE_OUT_OF_MEMORY);
                }

                Entry->ApicId = Record->Lapic.ApicId;
                Entry->AcpiId = Record->Lapic.AcpiId;
                Entry->IsX2Apic = 0;
                RtPushSList(&LapicListHead, &Entry->ListHeader);
                VidPrint(
                    KE_MESSAGE_DEBUG,
                    "Kernel APIC",
                    "added LAPIC %u (ACPI ID %u) to the list\n",
                    Entry->ApicId,
                    Entry->AcpiId);

                break;
            }

            case IOAPIC_RECORD: {
                IoapicEntry *Entry = MmAllocatePool(sizeof(IoapicEntry), "Apic");
                if (!Entry) {
                    VidPrint(
                        KE_MESSAGE_ERROR, "Kernel APIC", "couldn't allocate space for an IOAPIC\n");
                    KeFatalError(KE_OUT_OF_MEMORY);
                }

                Entry->Id = Record->Ioapic.IoapicId;
                Entry->GsiBase = Record->Ioapic.GsiBase;
                Entry->VirtualAddress = MI_PADDR_TO_VADDR(Record->Ioapic.Address);
                Entry->MaxRedirEntry = (ReadIoapicRegister(Entry, IOAPIC_VER_REG) >> 16) + 1;

                /* Set some sane defaults for all IOAPICs we find. */
                for (uint8_t i = 0; i < Entry->MaxRedirEntry; i++) {
                    WriteIoapicRegister(Entry, IOAPIC_REDIR_REG_LOW(i), 0x10000);
                    WriteIoapicRegister(Entry, IOAPIC_REDIR_REG_HIGH(i), 0);
                }

                RtPushSList(&IoapicListHead, &Entry->ListHeader);
                VidPrint(
                    KE_MESSAGE_DEBUG,
                    "Kernel APIC",
                    "added IOAPIC %hhu (GSI base %u, size %u) to the list\n",
                    Entry->Id,
                    Entry->GsiBase,
                    Entry->MaxRedirEntry);

                break;
            }

            case LAPIC_ADDRESS_OVERRIDE_RECORD: {
                LapicAddress = MI_PADDR_TO_VADDR(Record->LapicAddressOverride.Address);
                break;
            }

            case X2APIC_RECORD: {
                /* Prevent a bunch of entries with the same APIC ID (but probably different ACPI
                   IDs) filling our processor list. */
                if (GetLapic(Record->X2Apic.X2ApicId)) {
                    break;
                }

                LapicEntry *Entry = MmAllocatePool(sizeof(LapicEntry), "Apic");
                if (!Entry) {
                    VidPrint(
                        KE_MESSAGE_ERROR, "Kernel APIC", "couldn't allocate space for a x2APIC\n");
                    KeFatalError(KE_OUT_OF_MEMORY);
                }

                Entry->ApicId = Record->X2Apic.X2ApicId;
                Entry->AcpiId = Record->X2Apic.AcpiId;
                Entry->IsX2Apic = 1;
                RtPushSList(&LapicListHead, &Entry->ListHeader);
                VidPrint(
                    KE_MESSAGE_DEBUG,
                    "Kernel APIC",
                    "added x2APIC %u (ACPI ID %u) to the list\n",
                    Entry->ApicId,
                    Entry->AcpiId);

                break;
            }
        }

        Position += Record->Length;
    }

    /* The spec says that we might not always have to mask everything on the PIC, but we always
       do that anyways.
       The boot manager should have already remapped the IRQs (for handling early kernel
       exceptions). */
    WritePortByte(0x21, 0xFF);
    WritePortByte(0xA1, 0xFF);

    /* Hardware enable the Local APIC if it wasn't enabled. */
    WriteMsr(0x1B, ReadMsr(0x1B) | 0x800);

    /* And setup the remaining registers, this should finish enabling the LAPIC. */
    WriteLapicRegister(0x80, 0);
    WriteLapicRegister(0xF0, 0x1FF);

    Position = (char *)(Madt + 1);
    while (Position < (char *)Madt + Madt->Length) {
        MadtRecord *Record = (MadtRecord *)Position;
        if (Record->Type != IOAPIC_SOURCE_OVERRIDE_RECORD) {
            Position += Record->Length;
            continue;
        }

        IoapicOverrideEntry *Entry = MmAllocatePool(sizeof(IoapicOverrideEntry), "Apic");
        if (!Entry) {
            VidPrint(
                KE_MESSAGE_ERROR,
                "Kernel APIC",
                "couldn't allocate space for an IOAPIC source override\n");
            KeFatalError(KE_OUT_OF_MEMORY);
        }

        Entry->Irq = Record->IoapicSourceOverride.IrqSource;
        Entry->Gsi = Record->IoapicSourceOverride.Gsi;
        Entry->PinPolarity = (Record->IoapicSourceOverride.Flags & 2) != 0;
        Entry->TriggerMode = (Record->IoapicSourceOverride.Flags & 8) != 0;
        RtPushSList(&IoapicOverrideListHead, &Entry->ListHeader);
        VidPrint(
            KE_MESSAGE_DEBUG,
            "Kernel APIC",
            "added IOAPIC redir (IRQ %hhu, GSI %hhu) to the list\n",
            Entry->Irq,
            Entry->Gsi);

        Position += Record->Length;
    }

    __asm__ volatile("sti");
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function signals to the APIC that we're done handling an interrupt.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KiSendEoi(void) {
    WriteLapicRegister(0xB0, 0);
}
