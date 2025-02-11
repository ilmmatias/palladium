/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <cpuid.h>
#include <kernel/halp.h>
#include <kernel/intrin.h>
#include <kernel/ke.h>
#include <kernel/mm.h>
#include <kernel/vid.h>

RtSList HalpLapicListHead = {};

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
static HalpLapicEntry *GetLapic(uint32_t Id) {
    RtSList *ListHeader = HalpLapicListHead.Next;

    while (ListHeader) {
        HalpLapicEntry *Entry = CONTAINING_RECORD(ListHeader, HalpLapicEntry, ListHeader);

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
        return ReadMsr(HALP_APIC_REG_MSR + (Number >> 4));
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
        WriteMsr(HALP_APIC_REG_MSR + (Number >> 4), Data);
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
    uint32_t Register = HalpReadLapicRegister(HALP_APIC_ID_REG);
    return X2ApicEnabled ? Register : HALP_APIC_ID_VALUE(Register);
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
    HalpMadtHeader *Madt = KiFindAcpiTable("APIC", 0);
    if (!Madt) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_APIC_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_TABLE_NOT_FOUND,
            0,
            0);
    }

    uint32_t Eax, Ebx, Ecx, Edx;
    __cpuid(1, Eax, Ebx, Ecx, Edx);
    if (Ecx & 0x200000) {
        /* x2APIC uses MSRs instead of the LAPIC address, so Read/WriteRegister needs to know if we
           enabled it. */
        X2ApicEnabled = true;
        VidPrint(VID_MESSAGE_INFO, "Kernel HAL", "using x2APIC mode\n");
    } else {
        /* We're assuming xAPIC is available, but we should probably check if so as well. */
        VidPrint(VID_MESSAGE_INFO, "Kernel HAL", "using xAPIC mode\n");
    }

    char *Position = (char *)(Madt + 1);
    while (Position < (char *)Madt + Madt->Length) {
        HalpMadtRecord *Record = (HalpMadtRecord *)Position;

        switch (Record->Type) {
            case HALP_LAPIC_RECORD: {
                /* Prevent a bunch of entries with the same APIC ID (but probably different ACPI
                   IDs) filling our processor list. */
                if (GetLapic(Record->Lapic.ApicId) || !(Record->Lapic.Flags & 1)) {
                    break;
                }

                HalpLapicEntry *Entry = MmAllocatePool(sizeof(HalpLapicEntry), MM_POOL_TAG_APIC);
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

            case HALP_X2APIC_RECORD: {
                /* Prevent a bunch of entries with the same APIC ID (but probably different ACPI
                   IDs) filling our processor list. */
                if (GetLapic(Record->X2Apic.X2ApicId) || !(Record->X2Apic.Flags & 1)) {
                    break;
                }

                HalpLapicEntry *Entry = MmAllocatePool(sizeof(HalpLapicEntry), MM_POOL_TAG_APIC);
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
    } else if (HalpProcessorCount >= KE_MAX_PROCESSORS) {
        /* Over the processor mask bits limit, we'll just panic here. */
        KeFatalError(
            KE_PANIC_PROCESSOR_LIMIT_EXCEEDED, HalpProcessorCount, KE_MAX_PROCESSORS, 0, 0);
    }

    /* Default to the LAPIC address given in the MSR (if we're not using x2APIC). */
    if (!X2ApicEnabled) {
        LapicAddress = MmMapSpace(ReadMsr(HALP_APIC_MSR) & ~0xFFF, MM_PAGE_SIZE, MM_SPACE_IO);
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
    /* More than likely APIC is already enabled in the MSR, but just in case, set it up (activating
     * X2APIC if possible). */
    uint64_t Value = ReadMsr(HALP_APIC_MSR) | HALP_APIC_MSR_ENABLE;
    if (X2ApicEnabled) {
        WriteMsr(HALP_APIC_MSR, Value | HALP_APIC_MSR_X2APIC_ENABLE);
    } else {
        WriteMsr(HALP_APIC_MSR, Value);
    }

    /* Mask (disable) the legacy PIC. */
    WritePortByte(HALP_PIC_CMD1, HALP_PIC_ICW1);
    WritePortByte(HALP_PIC_CMD2, HALP_PIC_ICW1);
    WritePortByte(HALP_PIC_DATA1, HALP_PIC_ICW2_MASTER);
    WritePortByte(HALP_PIC_DATA2, HALP_PIC_ICW2_SLAVE);
    WritePortByte(HALP_PIC_DATA1, HALP_PIC_ICW3_MASTER);
    WritePortByte(HALP_PIC_DATA2, HALP_PIC_ICW3_SLAVE);
    WritePortByte(HALP_PIC_DATA1, HALP_PIC_ICW4_MASTER);
    WritePortByte(HALP_PIC_DATA2, HALP_PIC_ICW4_SLAVE);
    WritePortByte(HALP_PIC_DATA1, HALP_PIC_MASK);
    WritePortByte(HALP_PIC_DATA2, HALP_PIC_MASK);

    /* Mask out all LVT entries (the LAPIC timer will get unmasked later). */
    HalpApicLvtRecord Record = {0};
    Record.Masked = 1;

    /* These should always exist no matter the processor. */
    uint64_t MaxLvt = HALP_APIC_VER_MAX_LVT(HalpReadLapicRegister(HALP_APIC_VER_REG));
    HalpWriteLapicRegister(HALP_APIC_LVTT_REG, Record.RawData);
    HalpWriteLapicRegister(HALP_APIC_LVT0_REG, Record.RawData);
    HalpWriteLapicRegister(HALP_APIC_LVT1_REG, Record.RawData);
    HalpWriteLapicRegister(HALP_APIC_LVTERR_REG, Record.RawData);

    /* PMC and its associated interrupt were introduced in the P6. */
    if (MaxLvt >= 5) {
        HalpWriteLapicRegister(HALP_APIC_LVTPC_REG, Record.RawData);
    }

    /* THMR and its associated interrupt were introduced in the Pentium 4. */
    if (MaxLvt >= 6) {
        HalpWriteLapicRegister(HALP_APIC_LVTTHMR_REG, Record.RawData);
    }

    /* CMCI and its associated interrupt were introduced in the Xeon 5500. */
    if (MaxLvt >= 7) {
        HalpWriteLapicRegister(HALP_APIC_LVTCMCI_REG, Record.RawData);
    }

    /* Back to back ESR writes to clear it (probably just a single write already does the trick on
     * modern CPUs? but it doesn't hurt to do it). */
    HalpWriteLapicRegister(HALP_APIC_ESR_REG, 0);
    HalpWriteLapicRegister(HALP_APIC_ESR_REG, 0);

    /* LDR/DFR setup isn't needed when using physical destination mode. */
    HalpWriteLapicRegister(HALP_APIC_TPR_REG, KE_IRQL_PASSIVE);

    /* Now we can setup the spurious interrupt vector, and the enable the local APIC (we're safe to
     * receive interrupts after this). */
    HalpApicSpivRegister Register;
    Register.RawData = HalpReadLapicRegister(HALP_APIC_SPIV_REG);
    Register.Vector = HALP_INT_SPURIOUS_VECTOR;
    Register.Enable = true;
    HalpWriteLapicRegister(HALP_APIC_SPIV_REG, Register.RawData);
    HalpSendEoi();
    __asm__ volatile("sti");
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sends an interrupt to another processor.
 *
 * PARAMETERS:
 *     Target - APIC ID of the target.
 *     Vector - Interrupt vector/action we want to trigger in the target.
 *     DeliveryMode - Type of IPI to send (normal, SMI, NMI, etc).
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpSendIpi(uint32_t Target, uint8_t Vector, uint8_t DeliveryMode) {
    /* WRMSR into the X2APIC range is not a serializing instruction on Intel processors, so we
     * need to lfence+mfence for those. */
    if (X2ApicEnabled) {
        __asm__ volatile("mfence; lfence" : : : "memory");
    }

    /* We only support physical no-shorthand IPIs (in this function). */
    HalpApicCommandRegister Register = {0};
    Register.Vector = Vector;
    Register.DestinationMode = HALP_APIC_ICR_DESTINATION_MODE_PHYSICAL;
    Register.DestinationType = HALP_APIC_ICR_DESTINATION_TYPE_DEFAULT;

    /* INIT de-assert is mostly the same as an INIT, but with level+trigger set differently. */
    if (DeliveryMode == HALP_APIC_ICR_DELIVERY_INIT_DEASSERT) {
        Register.DeliveryMode = HALP_APIC_ICR_DELIVERY_INIT;
        Register.Level = HALP_APIC_ICR_LEVEL_DEASSERT;
        Register.TriggerMode = HAL_INT_TRIGGER_LEVEL;
    } else {
        Register.DeliveryMode = DeliveryMode;
        Register.Level = HALP_APIC_ICR_LEVEL_ASSERT;
        Register.TriggerMode = HALP_APIC_ICR_TRIGGER_EDGE;
    }

    if (X2ApicEnabled) {
        Register.DestinationFull = Target;
        HalpWriteLapicRegister(HALP_APIC_ICR_REG_LOW, Register.RawData);
    } else {
        Register.DestinationShort = Target;
        HalpWriteLapicRegister(HALP_APIC_ICR_REG_HIGH, Register.HighData);
        HalpWriteLapicRegister(HALP_APIC_ICR_REG_LOW, Register.LowData);
    }

    /* x2APIC doesn't has the DeliveryStatus bit (so no need to pool anything on it). */
    if (!X2ApicEnabled) {
        while (true) {
            PauseProcessor();
            HalpApicCommandRegister Register = {0};
            Register.LowData = HalpReadLapicRegister(HALP_APIC_ICR_REG_LOW);
            if (!Register.DeliveryStatus) {
                break;
            }
        }
    }
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
    HalpWriteLapicRegister(HALP_APIC_EOI_REG, 0);
}
