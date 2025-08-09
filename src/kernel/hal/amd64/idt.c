/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <os/intrin.h>
#include <kernel/ke.h>
#include <kernel/mm.h>
#include <rt/except.h>
#include <string.h>

extern struct __attribute__((packed)) {
    char Code[16];
} HalpDefaultInterruptHandlers[256];

extern void HalpDivisionTrapEntry(void);
extern void HalpDebugTrapEntry(void);
extern void HalpNmiTrapEntry(void);
extern void HalpBreakpointTrapEntry(void);
extern void HalpOverflowTrapEntry(void);
extern void HalpBoundRangeTrapEntry(void);
extern void HalpInvalidOpcodeTrapEntry(void);
extern void HalpNumericCoprocessorTrapEntry(void);
extern void HalpDoubleFaultTrapEntry(void);
extern void HalpSegmentOverrunTrapEntry(void);
extern void HalpInvalidTssTrapEntry(void);
extern void HalpSegmentNotPresentTrapEntry(void);
extern void HalpStackSegmentTrapEntry(void);
extern void HalpGeneralProtectionTrapEntry(void);
extern void HalpPageFaultTrapEntry(void);
extern void HalpReservedTrapEntry(void);
extern void HalpX87FloatingPointTrapEntry(void);
extern void HalpAlignmentCheckTrapEntry(void);
extern void HalpMachineCheckTrapEntry(void);
extern void HalpSimdFloatingPointTrapEntry(void);
extern void HalpVirtualizationTrapEntry(void);
extern void HalpControlProtectionTrapEntry(void);
extern void HalpHypervisorInjectionTrapEntry(void);
extern void HalpVmmCommunicationTrapEntry(void);
extern void HalpSecurityTrapEntry(void);
extern void HalpAlertEntry(void);
extern void HalpFastFailEntry(void);
extern void HalpDispatchEntry(void);
extern void HalpTimerEntry(void);
extern void HalpIpiEntry(void);
extern void HalpSpuriousEntry(void);

static struct {
    void (*Handler)(void);
    uint8_t Ist;
    uint8_t Dpl;
} TrapEntries[32] = {
    {HalpDivisionTrapEntry, DESCR_IST_NONE, DESCR_DPL_KERNEL},
    {HalpDebugTrapEntry, DESCR_IST_NONE, DESCR_DPL_USER},
    {HalpNmiTrapEntry, DESCR_IST_NMI, DESCR_DPL_KERNEL},
    {HalpBreakpointTrapEntry, DESCR_IST_NONE, DESCR_DPL_USER},
    {HalpOverflowTrapEntry, DESCR_IST_NONE, DESCR_DPL_KERNEL},
    {HalpBoundRangeTrapEntry, DESCR_IST_NONE, DESCR_DPL_KERNEL},
    {HalpInvalidOpcodeTrapEntry, DESCR_IST_NONE, DESCR_DPL_KERNEL},
    {HalpNumericCoprocessorTrapEntry, DESCR_IST_NONE, DESCR_DPL_KERNEL},
    {HalpDoubleFaultTrapEntry, DESCR_IST_DOUBLE_FAULT, DESCR_DPL_KERNEL},
    {HalpSegmentOverrunTrapEntry, DESCR_IST_NONE, DESCR_DPL_KERNEL},
    {HalpInvalidTssTrapEntry, DESCR_IST_NONE, DESCR_DPL_KERNEL},
    {HalpSegmentNotPresentTrapEntry, DESCR_IST_NONE, DESCR_DPL_KERNEL},
    {HalpStackSegmentTrapEntry, DESCR_IST_NONE, DESCR_DPL_KERNEL},
    {HalpGeneralProtectionTrapEntry, DESCR_IST_NONE, DESCR_DPL_KERNEL},
    {HalpPageFaultTrapEntry, DESCR_IST_NONE, DESCR_DPL_KERNEL},
    {HalpReservedTrapEntry, DESCR_IST_NONE, DESCR_DPL_KERNEL},
    {HalpX87FloatingPointTrapEntry, DESCR_IST_NONE, DESCR_DPL_KERNEL},
    {HalpAlignmentCheckTrapEntry, DESCR_IST_NONE, DESCR_DPL_KERNEL},
    {HalpMachineCheckTrapEntry, DESCR_IST_MACHINE_CHECK, DESCR_DPL_KERNEL},
    {HalpSimdFloatingPointTrapEntry, DESCR_IST_NONE, DESCR_DPL_KERNEL},
    {HalpVirtualizationTrapEntry, DESCR_IST_NONE, DESCR_DPL_KERNEL},
    {HalpControlProtectionTrapEntry, DESCR_IST_NONE, DESCR_DPL_KERNEL},
    {HalpReservedTrapEntry, DESCR_IST_NONE, DESCR_DPL_KERNEL},
    {HalpReservedTrapEntry, DESCR_IST_NONE, DESCR_DPL_KERNEL},
    {HalpReservedTrapEntry, DESCR_IST_NONE, DESCR_DPL_KERNEL},
    {HalpReservedTrapEntry, DESCR_IST_NONE, DESCR_DPL_KERNEL},
    {HalpReservedTrapEntry, DESCR_IST_NONE, DESCR_DPL_KERNEL},
    {HalpReservedTrapEntry, DESCR_IST_NONE, DESCR_DPL_KERNEL},
    {HalpHypervisorInjectionTrapEntry, DESCR_IST_NONE, DESCR_DPL_KERNEL},
    {HalpVmmCommunicationTrapEntry, DESCR_IST_NONE, DESCR_DPL_KERNEL},
    {HalpSecurityTrapEntry, DESCR_IST_NONE, DESCR_DPL_KERNEL},
    {HalpReservedTrapEntry, DESCR_IST_NONE, DESCR_DPL_KERNEL},
};

static volatile uint64_t GsiUsed[256] = {0};

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries handling a NMI (either as a processor freeze, or a machine check/panic).
 *
 * PARAMETERS:
 *     InterruptFrame - Current interrupt data.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpDispatchNmi(HalInterruptFrame *) {
    if (KeGetCurrentProcessor()->EventType == KE_EVENT_TYPE_FREEZE) {
        while (true) {
            StopProcessor();
        }
    }

    KeFatalError(KE_PANIC_NMI_HARDWARE_FAILURE, 0, 0, 0, 0);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function halts the system because of an unhandled trap.
 *
 * PARAMETERS:
 *     InterruptFrame - Current interrupt data.
 *     ExceptionCode - Which exception happened.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpDispatchTrap(HalInterruptFrame *InterruptFrame, uint64_t ExceptionCode) {
    KeFatalError(KE_PANIC_TRAP_NOT_HANDLED, ExceptionCode, InterruptFrame->ErrorCode, 0, 0);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries gracefully handling an exception (and continuing execution.
 *
 * PARAMETERS:
 *     ExceptionCode - Type of the exception.
 *     InterruptFrame - Volatile register data.
 *     ExceptionFrame - Non-volatile register data.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpDispatchException(
    uint32_t ExceptionCode,
    HalInterruptFrame *InterruptFrame,
    HalExceptionFrame *ExceptionFrame) {
    /* Build the exception record. */
    RtExceptionRecord ExceptionRecord;
    memset(&ExceptionRecord, 0, sizeof(RtExceptionRecord));
    ExceptionRecord.ExceptionCode = ExceptionCode;
    ExceptionRecord.ExceptionFlags = 0;
    ExceptionRecord.ExceptionRecord = NULL;
    ExceptionRecord.ExceptionAddress = (void *)InterruptFrame->Rip;
    ExceptionRecord.NumberOfParameters = 0;

    /* A few of the exception codes actually have special meanings (we have to parse them). */
    if (ExceptionCode == RT_EXC_FLT_INVALID_OPERATION) {
        if (InterruptFrame->Mxcsr & 0x01) {
            ExceptionCode = RT_EXC_FLT_INVALID_OPERATION;
        } else if (InterruptFrame->Mxcsr & 0x02) {
            ExceptionCode = RT_EXC_FLT_DENORMAL_OPERAND;
        } else if (InterruptFrame->Mxcsr & 0x04) {
            ExceptionCode = RT_EXC_FLT_DIVIDE_BY_ZERO;
        } else if (InterruptFrame->Mxcsr & 0x08) {
            ExceptionCode = RT_EXC_FLT_OVERFLOW;
        } else if (InterruptFrame->Mxcsr & 0x10) {
            ExceptionCode = RT_EXC_FLT_UNDERFLOW;
        } else if (InterruptFrame->Mxcsr & 0x20) {
            ExceptionCode = RT_EXC_FLT_INEXACT_RESULT;
        }
        ExceptionRecord.ExceptionCode = ExceptionCode;
    } else if (ExceptionCode == RT_EXC_GENERAL_PROTECTION_FAULT) {
        /* How exactly should we do this (or at least fill the exception data)? */
        ExceptionRecord.ExceptionCode = RT_EXC_ACCESS_VIOLATION;
        ExceptionRecord.NumberOfParameters = 2;
    } else if (ExceptionCode == RT_EXC_PAGE_FAULT) {
        ExceptionRecord.ExceptionCode = RT_EXC_ACCESS_VIOLATION;
        ExceptionRecord.NumberOfParameters = 2;
        ExceptionRecord.ExceptionInformation[0] = (void *)((InterruptFrame->ErrorCode & 0x02) >> 1);
        ExceptionRecord.ExceptionInformation[1] = (void *)InterruptFrame->FaultAddress;
    }

    /* And the unwind context (using both frames). */
    RtContext ContextRecord;
    ContextRecord.Rax = InterruptFrame->Rax;
    ContextRecord.Rcx = InterruptFrame->Rcx;
    ContextRecord.Rdx = InterruptFrame->Rdx;
    ContextRecord.Rbx = ExceptionFrame->Rbx;
    ContextRecord.Rsp = InterruptFrame->Rsp;
    ContextRecord.Rbp = ExceptionFrame->Rbp;
    ContextRecord.Rsi = ExceptionFrame->Rsi;
    ContextRecord.Rdi = ExceptionFrame->Rdi;
    ContextRecord.R8 = InterruptFrame->R8;
    ContextRecord.R9 = InterruptFrame->R9;
    ContextRecord.R10 = InterruptFrame->R10;
    ContextRecord.R11 = InterruptFrame->R11;
    ContextRecord.R12 = ExceptionFrame->R12;
    ContextRecord.R13 = ExceptionFrame->R13;
    ContextRecord.R14 = ExceptionFrame->R14;
    ContextRecord.R15 = ExceptionFrame->R15;
    ContextRecord.Xmm0 = InterruptFrame->Xmm0;
    ContextRecord.Xmm1 = InterruptFrame->Xmm1;
    ContextRecord.Xmm2 = InterruptFrame->Xmm2;
    ContextRecord.Xmm3 = InterruptFrame->Xmm3;
    ContextRecord.Xmm4 = InterruptFrame->Xmm4;
    ContextRecord.Xmm5 = InterruptFrame->Xmm5;
    ContextRecord.Xmm6 = ExceptionFrame->Xmm6;
    ContextRecord.Xmm7 = ExceptionFrame->Xmm7;
    ContextRecord.Xmm8 = ExceptionFrame->Xmm8;
    ContextRecord.Xmm9 = ExceptionFrame->Xmm9;
    ContextRecord.Xmm10 = ExceptionFrame->Xmm10;
    ContextRecord.Xmm11 = ExceptionFrame->Xmm11;
    ContextRecord.Xmm12 = ExceptionFrame->Xmm12;
    ContextRecord.Xmm13 = ExceptionFrame->Xmm13;
    ContextRecord.Xmm14 = ExceptionFrame->Xmm14;
    ContextRecord.Xmm15 = ExceptionFrame->Xmm15;
    ContextRecord.Rip = InterruptFrame->Rip;
    ContextRecord.Rflags = InterruptFrame->Rflags;

    if (RtDispatchException(&ExceptionRecord, &ContextRecord)) {
        /* CONTINUE_EXECUTION, just rebuild the frames and return from the interrupt. */
        InterruptFrame->Rax = ContextRecord.Rax;
        InterruptFrame->Rcx = ContextRecord.Rcx;
        InterruptFrame->Rdx = ContextRecord.Rdx;
        ExceptionFrame->Rbx = ContextRecord.Rbx;
        InterruptFrame->Rsp = ContextRecord.Rsp;
        ExceptionFrame->Rbp = ContextRecord.Rbp;
        ExceptionFrame->Rsi = ContextRecord.Rsi;
        ExceptionFrame->Rdi = ContextRecord.Rdi;
        InterruptFrame->R8 = ContextRecord.R8;
        InterruptFrame->R9 = ContextRecord.R9;
        InterruptFrame->R10 = ContextRecord.R10;
        InterruptFrame->R11 = ContextRecord.R11;
        ExceptionFrame->R12 = ContextRecord.R12;
        ExceptionFrame->R13 = ContextRecord.R13;
        ExceptionFrame->R14 = ContextRecord.R14;
        ExceptionFrame->R15 = ContextRecord.R15;
        InterruptFrame->Xmm0 = ContextRecord.Xmm0;
        InterruptFrame->Xmm1 = ContextRecord.Xmm1;
        InterruptFrame->Xmm2 = ContextRecord.Xmm2;
        InterruptFrame->Xmm3 = ContextRecord.Xmm3;
        InterruptFrame->Xmm4 = ContextRecord.Xmm4;
        InterruptFrame->Xmm5 = ContextRecord.Xmm5;
        ExceptionFrame->Xmm6 = ContextRecord.Xmm6;
        ExceptionFrame->Xmm7 = ContextRecord.Xmm7;
        ExceptionFrame->Xmm8 = ContextRecord.Xmm8;
        ExceptionFrame->Xmm9 = ContextRecord.Xmm9;
        ExceptionFrame->Xmm10 = ContextRecord.Xmm10;
        ExceptionFrame->Xmm11 = ContextRecord.Xmm11;
        ExceptionFrame->Xmm12 = ContextRecord.Xmm12;
        ExceptionFrame->Xmm13 = ContextRecord.Xmm13;
        ExceptionFrame->Xmm14 = ContextRecord.Xmm14;
        ExceptionFrame->Xmm15 = ContextRecord.Xmm15;
        InterruptFrame->Rip = ContextRecord.Rip;
        InterruptFrame->Rflags = ContextRecord.Rflags;
        return;
    }

    /* Page faults have a different format. */
    if (ExceptionCode == RT_EXC_PAGE_FAULT) {
        KeFatalError(
            KE_PANIC_PAGE_FAULT_NOT_HANDLED,
            (uint64_t)InterruptFrame->FaultAddress,
            InterruptFrame->ErrorCode,
            0,
            0);
    } else {
        KeFatalError(KE_PANIC_TRAP_NOT_HANDLED, ExceptionCode, InterruptFrame->ErrorCode, 0, 0);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries running all registered/enabled handlers for the current interrupt.
 *
 * PARAMETERS:
 *     InterruptFrame - Current interrupt data.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpDispatchInterrupt(HalInterruptFrame *InterruptFrame) {
    KeProcessor *Processor = KeGetCurrentProcessor();
    RtDList *HandlerList = &Processor->InterruptList[InterruptFrame->InterruptNumber];

    for (RtDList *ListHeader = HandlerList->Next; ListHeader != HandlerList;
         ListHeader = ListHeader->Next) {
        HalInterrupt *Interrupt = CONTAINING_RECORD(ListHeader, HalInterrupt, ListHeader);
        KeAcquireSpinLockAtCurrentIrql(&Interrupt->Lock);
        Interrupt->Handler(Interrupt->HandlerContext);
        KeReleaseSpinLockAtCurrentIrql(&Interrupt->Lock);
    }

    HalpSendEoi();
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes an entry inside the IDT.
 *
 * PARAMETERS:
 *     Processor - Pointer to the processor-specific structure.
 *     EntryIndex - Which entry to initialize.
 *     Base - Virtual address of the ISR.
 *     Segment - Target segment for the ISR.
 *     IstIndex - Which of the TSS IST stacks we should use for this (==0 means kernel stack).
 *     Type - Type of the descriptor.
 *     Dpl - Privilege level of the descriptor.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void InitializeEntry(
    KeProcessor *Processor,
    uint8_t EntryIndex,
    uint64_t Base,
    uint16_t Segment,
    uint8_t IstIndex,
    uint8_t Type,
    uint8_t Dpl) {
    HalpIdtEntry *Entry = &Processor->IdtEntries[EntryIndex];
    Entry->BaseLow = Base & 0xFFFF;
    Entry->Segment = Segment;
    Entry->IstIndex = IstIndex;
    Entry->Reserved0 = 0;
    Entry->Type = Type;
    Entry->Reserved1 = 0;
    Entry->Dpl = Dpl;
    Entry->Present = 1;
    Entry->BaseMiddle = (Base >> 16) & 0xFFFF;
    Entry->BaseHigh = (Base >> 32) & 0xFFFFFFFF;
    Entry->Reserved2 = 0;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function starts the interrupt handler setup process (so that we'll already be handling
 *     traps).
 *
 * PARAMETERS:
 *     Processor - Pointer to the processor-specific structure.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpInitializeIdt(KeProcessor *Processor) {
    for (int i = 0; i < 256; i++) {
        uint64_t Base = 0;
        uint8_t Ist = DESCR_IST_NONE;
        uint8_t Dpl = DESCR_DPL_KERNEL;

        /* Exception handlers. */
        if (i < 32) {
            Base = (uint64_t)TrapEntries[i].Handler;
            Ist = TrapEntries[i].Ist;
            Dpl = TrapEntries[i].Dpl;
        }

        /* Interrupt handlers with special conditions (reserved IRQL, or not following the standard
           rules like enabling interrupts before enabling). */
        else if (i == HALP_INT_ALERT_VECTOR) {
            Base = (uint64_t)HalpAlertEntry;
        } else if (i == HALP_INT_FASTFAIL_VECTOR) {
            Base = (uint64_t)HalpFastFailEntry;
        } else if (i == HALP_INT_DISPATCH_VECTOR) {
            Base = (uint64_t)HalpDispatchEntry;
        } else if (i == HALP_INT_TIMER_VECTOR) {
            Base = (uint64_t)HalpTimerEntry;
        } else if (i == HALP_INT_IPI_VECTOR) {
            Base = (uint64_t)HalpIpiEntry;
        } else if (i == HALP_INT_SPURIOUS_VECTOR) {
            Base = (uint64_t)HalpSpuriousEntry;
        }

        /* Default IRQ handlers for everything else. */
        else {
            Base = (uint64_t)&HalpDefaultInterruptHandlers[i];
        }

        InitializeEntry(Processor, i, Base, DESCR_SEG_KCODE, Ist, IDT_TYPE_INT, Dpl);
        RtInitializeDList(&Processor->InterruptList[i]);
    }

    HalpIdtDescriptor Descriptor;
    Descriptor.Limit = sizeof(Processor->IdtEntries) - 1;
    Descriptor.Base = (uint64_t)Processor->IdtEntries;
    __asm__ volatile("lidt %0" : : "m"(Descriptor));
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the interrupt data structure for future use when enabling an
 *     interrupt handler.
 *     If the PinPolarity and TriggerMode values are HAL_INT_<POLARITY/TRIGGER>_UNSET (which should
 *     be the case for an empty bus vector value), you can either edit them into what the device
 *     prefers, or leave it unset for the default configuration.
 *
 * PARAMETERS:
 *     Data - Which structure to initialize.
 *     BusVector - A special (arch/machine-dependent) number representing where the interrupt will
 *                 come from. This can be HAL_INT_VECTOR_UNSET if the device doesn't has any default
 *                 interrupt vector it will come from.
 *
 * RETURN VALUE:
 *     true if the an interrupt vector was successfully allocated for usage.
 *-----------------------------------------------------------------------------------------------*/
bool HalInitializeInterruptData(HalInterruptData *Data, uint32_t BusVector) {
    memset(Data, 0, sizeof(HalInterruptData));
    Data->PinPolarity = HAL_INT_POLARITY_UNSET;
    Data->TriggerMode = HAL_INT_TRIGGER_UNSET;

    /* For "legacy" devices (those discovered via non-standard ways, or that have a fixed IRQ
     * they always come from), we need to map the IRQ into a GSI (via the IOAPIC override
     * list). If the mapping can't be done (no override for it), then the IRQ itself should be the
     * GSI. */
    if (BusVector != HAL_INT_VECTOR_UNSET) {
        if (!HalpTranslateIrq(
                BusVector, &Data->SourceGsi, &Data->PinPolarity, &Data->TriggerMode)) {
            Data->SourceGsi = BusVector;
        }

        if (__atomic_fetch_or(&GsiUsed[Data->SourceGsi], 0x01, __ATOMIC_ACQUIRE) & 0x01) {
            return false;
        } else {
            Data->HasGsi = true;
            return true;
        }
    }

    /* For all other cases, we need to allocate a new unused GSI. */
    for (size_t i = 0; i < 256; i++) {
        if (!(__atomic_fetch_or(&GsiUsed[i], 0x01, __ATOMIC_ACQUIRE) & 0x01)) {
            Data->HasGsi = true;
            Data->SourceGsi = i;
            return true;
        }
    }

    return false;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function allocates a new interrupt vector to be used with HalCreateInterrupt and its
 *     friends.
 *
 * PARAMETERS:
 *     Data - Which interrupt data struct to store the new vector.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalAllocateInterruptVector(HalInterruptData *Data) {
    KeProcessor *Processor = KeGetCurrentProcessor();
    KeIrql OldIrql = KeAcquireSpinLockAndRaiseIrql(&Processor->Lock, KE_IRQL_SYNCH);
    uint8_t LowestCount = UINT8_MAX;
    size_t LowestVector = 0;

    Data->HasVector = true;

    for (KeIrql Irql = KE_IRQL_TIMER - 1; Irql >= KE_IRQL_DEVICE; Irql--) {
        for (uint8_t Offset = 0; Offset < 16; Offset++) {
            uint8_t Vector = (Irql << 4) | Offset;

            /* Empty vectors always get chosen if possible. */
            if (!Processor->InterruptUsage[Vector]) {
                Data->TargetVector = Vector;
                Data->Irql = Irql;
                Processor->InterruptUsage[Vector]++;
                KeReleaseSpinLockAndLowerIrql(&Processor->Lock, OldIrql);
                return;
            }

            /* Otherwise, keep count of which vector has the least usage. */
            if (Processor->InterruptUsage[Vector] < LowestCount) {
                LowestCount = Processor->InterruptUsage[Vector];
                LowestVector = Vector;
            }
        }
    }

    /* As long we're inside the valid IRQL range, any vector should be usable. */
    Data->TargetVector = LowestVector;
    Data->Irql = LowestVector >> 4;
    Processor->InterruptUsage[LowestVector]++;
    KeReleaseSpinLockAndLowerIrql(&Processor->Lock, OldIrql);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function releases the resources previously allocated by HalInitializeInterruptData and
 *     HalAllocateInterruptVector. We should only be directly called if something fails before
 *     HalCreateInterrupt gets called, otherwise, you should call HalDeleteInterrupt instead!
 *
 * PARAMETERS:
 *     Data - Which interrupt data struct to release the resources from.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalReleaseInterruptData(HalInterruptData *Data) {
    KeProcessor *Processor = KeGetCurrentProcessor();
    KeIrql OldIrql = KeAcquireSpinLockAndRaiseIrql(&Processor->Lock, KE_IRQL_SYNCH);

    if (Data->HasGsi) {
        __atomic_store_n(&GsiUsed[Data->SourceGsi], 0, __ATOMIC_RELEASE);
    }

    if (Data->HasVector) {
        Processor->InterruptUsage[Data->TargetVector]--;
    }

    KeReleaseSpinLockAndLowerIrql(&Processor->Lock, OldIrql);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates and initializes a new interrupt object, getting it ready for
 *     HalEnableInterrupt.
 *
 * PARAMETERS:
 *     Data - Interrupt data previously initialized by HalInitializeInterruptData and
 *            HalAllocateInterruptVector.
 *     Handler - Function to be called when the interrupt gets triggered.
 *     HandlerContext - Data to be provided to the interrupt handler when it gets triggered.
 *
 * RETURN VALUE:
 *     Either the interrupt object, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
HalInterrupt *
HalCreateInterrupt(HalInterruptData *Data, void (*Handler)(void *), void *HandlerContext) {
    HalInterrupt *Interrupt = MmAllocatePool(sizeof(HalInterrupt), MM_POOL_TAG_INTERRUPT);
    if (!Interrupt) {
        return NULL;
    }

    Interrupt->Enabled = false;
    Interrupt->Lock = 0;
    Interrupt->Handler = Handler;
    Interrupt->HandlerContext = HandlerContext;
    memcpy(&Interrupt->Data, Data, sizeof(HalInterruptData));

    if (Interrupt->Data.PinPolarity == HAL_INT_POLARITY_UNSET) {
        Interrupt->Data.PinPolarity = HAL_INT_POLARITY_HIGH;
    }

    if (Interrupt->Data.TriggerMode == HAL_INT_TRIGGER_UNSET) {
        Interrupt->Data.PinPolarity = HAL_INT_TRIGGER_EDGE;
    }

    return Interrupt;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function releases the resources previously allocated by HalCreateInterrupt,
 *     automatically disabling the interrupt if required.
 *
 * PARAMETERS:
 *     Interrupt - Target interrupt to be deleted.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalDeleteInterrupt(HalInterrupt *Interrupt) {
    /* The first thing DisableInterrupt() does is check if the interrupt is already enabled, so this
     * should be okay. */
    HalDisableInterrupt(Interrupt);

    /* Now we just need to release the interrupt data resources, and free up the interrupt struct
     * itself. */
    HalReleaseInterruptData(&Interrupt->Data);
    MmFreePool(Interrupt, MM_POOL_TAG_INTERRUPT);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to enable the handling of the given interrupt.
 *
 * PARAMETERS:
 *     Interrupt - Target interrupt to be enabled.
 *
 * RETURN VALUE:
 *     false if the interrupt couldn't be registered, true otherwise.
 *-----------------------------------------------------------------------------------------------*/
bool HalEnableInterrupt(HalInterrupt *Interrupt) {
    /* We'll be assuming that if the interrupt is already enabled, the caller would rather
     * receive a "success" than a failure (as a previous call with the exact same interrupt
     * object did succeed). */
    if (Interrupt->Enabled) {
        return true;
    }

    /* On the other hand, if the interrupt data hasn't been initialized properly, we'll bail out. */
    if (!Interrupt->Data.HasGsi || !Interrupt->Data.HasVector) {
        return false;
    }

    KeProcessor *Processor = KeGetCurrentProcessor();
    KeIrql OldIrql = KeAcquireSpinLockAndRaiseIrql(&Processor->Lock, KE_IRQL_SYNCH);
    RtDList *Handlers = &Processor->InterruptList[Interrupt->Data.TargetVector];

    if (Handlers->Next != Handlers) {
        /* We already have one or more interrupt handlers on this vector, validate if we are
         * compatible with the already installed handlers. */
        HalInterrupt *FirstHandler = CONTAINING_RECORD(Handlers->Next, HalInterrupt, ListHeader);
        if (FirstHandler->Data.PinPolarity != Interrupt->Data.PinPolarity ||
            FirstHandler->Data.TriggerMode != Interrupt->Data.TriggerMode) {
            KeReleaseSpinLockAndLowerIrql(&Processor->Lock, OldIrql);
            return false;
        }
    }

    RtAppendDList(Handlers, &Interrupt->ListHeader);
    KeReleaseSpinLockAndLowerIrql(&Processor->Lock, OldIrql);
    HalpEnableGsi(
        Interrupt->Data.SourceGsi,
        Interrupt->Data.TargetVector,
        Interrupt->Data.PinPolarity,
        Interrupt->Data.TriggerMode);
    Interrupt->Enabled = true;
    return true;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function disables the handling of the given interrupt.
 *
 * PARAMETERS:
 *     Interrupt - Target interrupt to be disabled.
 *
 * RETURN VALUE:
 *     None
 *-----------------------------------------------------------------------------------------------*/
void HalDisableInterrupt(HalInterrupt *Interrupt) {
    if (Interrupt->Enabled) {
        return;
    }

    /* Should be as simple as marking us as not enabled + unlinking from the interrupt handler
     * list. */
    KeProcessor *Processor = KeGetCurrentProcessor();
    KeIrql OldIrql = KeAcquireSpinLockAndRaiseIrql(&Processor->Lock, KE_IRQL_SYNCH);
    RtUnlinkDList(&Interrupt->ListHeader);
    KeReleaseSpinLockAndLowerIrql(&Processor->Lock, OldIrql);
    HalpDisableGsi(Interrupt->Data.SourceGsi);
    Interrupt->Enabled = false;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function enters a critical code path (no interrupts allowed).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Arch-specific context.
 *-----------------------------------------------------------------------------------------------*/
void *HalpEnterCriticalSection(void) {
    uint64_t Flags;
    __asm__ volatile("pushfq; cli; pop %0" : "=r"(Flags) : : "memory");
    return (void *)Flags;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function marks the end of a critical code path (interrupts allowed again).
 *
 * PARAMETERS:
 *     Context - Previously returned value from HalpEnterCriticalSection.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpLeaveCriticalSection(void *Context) {
    __asm__ volatile("push %0; popf" : : "rm"(Context) : "memory", "cc");
}
