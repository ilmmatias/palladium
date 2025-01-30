/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <amd64/apic.h>
#include <amd64/msr.h>
#include <amd64/port.h>
#include <cpuid.h>
#include <hal.h>
#include <ke.h>
#include <mm.h>
#include <vid.h>

RtSList HalpLapicListHead = {};
uint32_t HalpProcessorCount = 0;

static void *LapicAddress = NULL;
static bool X2ApicEnabled = false;

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
    RtSList *ListHeader = HalpLapicListHead.Next;

    while (ListHeader) {
        LapicEntry *Entry = CONTAINING_RECORD(ListHeader, LapicEntry, ListHeader);

        if (Entry->ApicId == Id) {
            return Entry;
        }

        ListHeader = ListHeader->Next;
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
uint64_t HalpReadLapicRegister(uint32_t Number) {
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
void HalpWriteLapicRegister(uint32_t Number, uint64_t Data) {
    if (X2ApicEnabled) {
        WriteMsr(0x800 + (Number >> 4), Data);
    } else {
        *(volatile uint32_t *)((char *)LapicAddress + Number) = Data;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function obtains the APIC ID for the current processor.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     APIC ID for the current processor.
 *-----------------------------------------------------------------------------------------------*/
uint32_t HalpReadLapicId(void) {
    uint32_t Register = HalpReadLapicRegister(0x20);
    return X2ApicEnabled ? Register : (Register >> 24);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function parses the APIC/MADT table, collecting all information required to enable the
 *     Local APIC. and gets the system ready to handle interrupts
 *     (and other processors) using the Local APIC.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpInitializeApic(void) {
    MadtHeader *Madt = KiFindAcpiTable("APIC", 0);
    if (!Madt) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_APIC_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_TABLE_NOT_FOUND,
            0,
            0);
    }

    /* x2APIC uses MSRs instead of the LAPIC address, so Read/WriteRegister needs to know if we
       enabled it. */
    uint32_t Eax, Ebx, Ecx, Edx;
    __cpuid(1, Eax, Ebx, Ecx, Edx);
    if (Ecx & 0x200000) {
        X2ApicEnabled = true;
    }

    char *Position = (char *)(Madt + 1);
    while (Position < (char *)Madt + Madt->Length) {
        MadtRecord *Record = (MadtRecord *)Position;

        switch (Record->Type) {
            case LAPIC_RECORD: {
                /* Prevent a bunch of entries with the same APIC ID (but probably different ACPI
                   IDs) filling our processor list. */
                if (GetLapic(Record->Lapic.ApicId) || !(Record->Lapic.Flags & 1)) {
                    break;
                }

                LapicEntry *Entry = MmAllocatePool(sizeof(LapicEntry), "Apic");
                if (!Entry) {
                    KeFatalError(
                        KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
                        KE_PANIC_PARAMETER_APIC_INITIALIZATION_FAILURE,
                        KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
                        0,
                        0);
                }

                Entry->ApicId = Record->Lapic.ApicId;
                Entry->AcpiId = Record->Lapic.AcpiId;
                Entry->IsX2Apic = false;
                RtPushSList(&HalpLapicListHead, &Entry->ListHeader);
                VidPrint(
                    VID_MESSAGE_DEBUG,
                    "Kernel HAL",
                    "found LAPIC %u (ACPI ID %u)\n",
                    Entry->ApicId,
                    Entry->AcpiId);

                HalpProcessorCount++;
                break;
            }

            case X2APIC_RECORD: {
                /* Prevent a bunch of entries with the same APIC ID (but probably different ACPI
                   IDs) filling our processor list. */
                if (GetLapic(Record->X2Apic.X2ApicId) || !(Record->X2Apic.Flags & 1)) {
                    break;
                }

                LapicEntry *Entry = MmAllocatePool(sizeof(LapicEntry), "Apic");
                if (!Entry) {
                    KeFatalError(
                        KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
                        KE_PANIC_PARAMETER_APIC_INITIALIZATION_FAILURE,
                        KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
                        0,
                        0);
                }

                Entry->ApicId = Record->X2Apic.X2ApicId;
                Entry->AcpiId = Record->X2Apic.AcpiId;
                Entry->IsX2Apic = true;
                RtPushSList(&HalpLapicListHead, &Entry->ListHeader);
                VidPrint(
                    VID_MESSAGE_DEBUG,
                    "Kernel HAL",
                    "found x2APIC %u (ACPI ID %u)\n",
                    Entry->ApicId,
                    Entry->AcpiId);

                HalpProcessorCount++;
                break;
            }
        }

        Position += Record->Length;
    }

    if (HalpProcessorCount < 1) {
        HalpProcessorCount = 1;
    }

    /* Default to the LAPIC address given in the MSR (if we're not using x2APIC). */
    if (!X2ApicEnabled) {
        LapicAddress = MmMapSpace(ReadMsr(0x1B) & ~0xFFF, MM_PAGE_SIZE);
        if (!LapicAddress) {
            KeFatalError(
                KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_APIC_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
                0,
                0);
        }
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function enables interrupt handling using the Local APIC.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpEnableApic(void) {
    if (X2ApicEnabled) {
        WriteMsr(0x1B, (ReadMsr(0x1B) & ~0xFFF) | 0xC00);
    } else {
        WriteMsr(0x1B, (ReadMsr(0x1B) & ~0xFFF) | 0x800);
    }

    /* Reconfigure and mask the PIC (this seems to be necessary on all processors, or things can
     * get a bit hairy). */
    WritePortByte(0x21, 0xFF);
    WritePortByte(0xA1, 0xFF);

    /* And setup the remaining registers, this should finish enabling the LAPIC. */
    HalpWriteLapicRegister(0x80, 0);
    HalpWriteLapicRegister(0xF0, 0x1FF);

    __asm__ volatile("sti");
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sends an IPI to another processor.
 *
 * PARAMETERS:
 *     Target - APIC ID of the target.
 *     Vector - Interrupt vector/action we want to trigger in the target.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpSendIpi(uint32_t Target, uint32_t Vector) {
    if (X2ApicEnabled) {
        HalpWriteLapicRegister(0x300, ((uint64_t)Target << 32) | Vector);
    } else {
        HalpWriteLapicRegister(0x310, Target << 24);
        HalpWriteLapicRegister(0x300, Vector);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sends an non-maskable interrupt to another processor.
 *
 * PARAMETERS:
 *     Target - APIC ID of the target.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpSendNmi(uint32_t Target) {
    if (X2ApicEnabled) {
        HalpWriteLapicRegister(0x300, ((uint64_t)Target << 32) | 0x400);
    } else {
        HalpWriteLapicRegister(0x310, Target << 24);
        HalpWriteLapicRegister(0x300, 0x400);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function busy loops until the Local APIC says a previously sent IPI was delivered.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpWaitIpiDelivery(void) {
    while (HalpReadLapicRegister(0x300) & 0x1000)
        ;
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
    HalpWriteLapicRegister(0xB0, 0);
}
