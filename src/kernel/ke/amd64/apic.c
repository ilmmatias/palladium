/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <amd64/apic.h>
#include <cpuid.h>
#include <ke.h>
#include <mm.h>
#include <vid.h>

static RtSList LapicListHead = {};
static RtSList IoapicListHead = {};
static void *LapicAddress = NULL;
static int X2ApicEnabled = 0;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reads the specified MSR leaf.
 *
 * PARAMETERS:
 *     Number - Which leaf we should read.
 *
 * RETURN VALUE:
 *     What RDMSR returned.
 *-----------------------------------------------------------------------------------------------*/
static uint64_t ReadMsr(uint32_t Number) {
    uint32_t LowPart;
    uint32_t HighPart;
    __asm__ volatile("rdmsr" : "=a"(LowPart), "=d"(HighPart) : "c"(Number));
    return LowPart | ((uint64_t)HighPart << 32);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function writes into the specified MSR leaf.
 *
 * PARAMETERS:
 *     Number - Which leaf we should write into.
 *     Value - What we want to write.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void WriteMsr(uint32_t Number, uint64_t Value) {
    __asm__ volatile("wrmsr" ::"a"((uint32_t)Value), "c"(Number), "d"(Value >> 32));
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
 *     This function disables the given IOAPIC IRQ.
 *
 * PARAMETERS:
 *     Entry - Header containing the IOAPIC entry info.
 *     Irq - Which IRQ we wish to disable.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void MaskIoapicVector(IoapicEntry *Entry, uint8_t Irq) {
    WriteIoapicRegister(Entry, IOAPIC_REDIR_REG_LOW(Irq), 0x10000);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function enables and sets up the given IOAPIC IRQ.
 *
 * PARAMETERS:
 *     Entry - Header containing the IOAPIC entry info.
 *     Irq - Which IRQ we wish to enable.
 *     TargetVector - Which vector should be raised on the target CPU once this IRQ triggers.
 *     ApicId - Which CPU wants to handle this vector.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void
UnmaskIoapicVector(IoapicEntry *Entry, uint8_t Irq, uint8_t TargetVector, uint8_t ApicId) {
    WriteIoapicRegister(Entry, IOAPIC_REDIR_REG_LOW(Irq), TargetVector);
    WriteIoapicRegister(Entry, IOAPIC_REDIR_REG_HIGH(Irq), (uint32_t)ApicId << 24);
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

    /* We're setting up the interrupt controller, we should probably only enable interrupts after
       we're done. */
    __asm__ volatile("cli");
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

                Entry->IoapicId = Record->Ioapic.IoapicId;
                Entry->GsiBase = Record->Ioapic.GsiBase;
                Entry->VirtualAddress = MI_PADDR_TO_VADDR(Record->Ioapic.Address);

                Entry->ApicId = (ReadIoapicRegister(Entry, IOAPIC_ID_REG) >> 24) & 0xF0;
                Entry->MaxRedirEntry = (ReadIoapicRegister(Entry, IOAPIC_VER_REG) >> 16) + 1;

                /* Set some sane defaults for all IOAPICs we find. */
                for (uint8_t i = 0; i < Entry->MaxRedirEntry; i++) {
                    MaskIoapicVector(Entry, i);
                }

                RtPushSList(&IoapicListHead, &Entry->ListHeader);
                VidPrint(
                    KE_MESSAGE_DEBUG,
                    "Kernel APIC",
                    "added IOAPIC %hhu (APIC %u, GSI base %u, size %u) to the list\n",
                    Entry->IoapicId,
                    Entry->ApicId,
                    Entry->GsiBase,
                    Entry->MaxRedirEntry);

                break;
            }

            case X2APIC_RECORD: {
                LapicEntry *Entry = MmAllocatePool(sizeof(LapicEntry), "Apic");
                if (!Entry) {
                    VidPrint(
                        KE_MESSAGE_ERROR, "Kernel APIC", "couldn't allocate space for a x2APIC\n");
                    KeFatalError(KE_OUT_OF_MEMORY);
                }

                Entry->ApicId = Record->Lapic.ApicId;
                Entry->AcpiId = Record->Lapic.AcpiId;
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

    (void)UnmaskIoapicVector;
}
