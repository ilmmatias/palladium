/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <amd64/apic.h>
#include <amd64/msr.h>
#include <amd64/port.h>
#include <cpuid.h>
#include <hal.h>
#include <ke.h>
#include <mm.h>
#include <vid.h>

static RtSList LapicListHead = {};
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
static uint32_t ReadLapicRegister(uint32_t Number) {
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
 *     This function parses the APIC/MADT table, and gets the system ready to handle interrupts
 *     (and other processors) using the Local APIC.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpInitializeApic(void) {
    MadtHeader *Madt = HalFindAcpiTable("APIC", 0);
    if (!Madt) {
        VidPrint(KE_MESSAGE_ERROR, "Kernel HAL", "couldn't find the MADT table\n");
        KeFatalError(KE_BAD_ACPI_TABLES);
    }

    LapicAddress = MI_PADDR_TO_VADDR(Madt->LapicAddress);

    /* x2APIC uses MSRs instead of the LAPIC address, so Read/WriteRegister needs to know if we
       enabled it. */
    uint32_t Eax, Ebx, Ecx, Edx;
    __cpuid(1, Eax, Ebx, Ecx, Edx);
    if (Ecx & 0x200000) {
        WriteMsr(0x1B, ReadMsr(0x1B) | 0xC00);
        X2ApicEnabled = 1;
    }

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
                        KE_MESSAGE_ERROR, "Kernel HAL", "couldn't allocate space for a LAPIC\n");
                    KeFatalError(KE_OUT_OF_MEMORY);
                }

                Entry->ApicId = Record->Lapic.ApicId;
                Entry->AcpiId = Record->Lapic.AcpiId;
                Entry->IsX2Apic = 0;
                RtPushSList(&LapicListHead, &Entry->ListHeader);
                VidPrint(
                    KE_MESSAGE_INFO,
                    "Kernel HAL",
                    "found LAPIC %u (ACPI ID %u)\n",
                    Entry->ApicId,
                    Entry->AcpiId);

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
                        KE_MESSAGE_ERROR, "Kernel HAL", "couldn't allocate space for a x2APIC\n");
                    KeFatalError(KE_OUT_OF_MEMORY);
                }

                Entry->ApicId = Record->X2Apic.X2ApicId;
                Entry->AcpiId = Record->X2Apic.AcpiId;
                Entry->IsX2Apic = 1;
                RtPushSList(&LapicListHead, &Entry->ListHeader);
                VidPrint(
                    KE_MESSAGE_INFO,
                    "Kernel HAL",
                    "found x2APIC %u (ACPI ID %u)\n",
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

    __asm__ volatile("sti");
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function grabs the APIC ID of the current processor.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     APIC ID of the current processor.
 *-----------------------------------------------------------------------------------------------*/
uint32_t HalpGetCurrentApicId(void) {
    return ReadLapicRegister(0x20);
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
void HalpSendEoi(void) {
    WriteLapicRegister(0xB0, 0);
}
