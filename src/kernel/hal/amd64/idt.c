/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <amd64/halp.h>
#include <rt/except.h>
#include <string.h>

extern struct __attribute__((packed)) {
    char PushOp[2];
    char JumpOp[5];
} HalpDefaultInterruptHandlers[224];

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
extern void HalpDispatchEntry(void);
extern void HalpTimerEntry(void);

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
    if (HalGetCurrentProcessor()->EventStatus == KE_EVENT_FREEZE) {
        while (1) {
            HalpStopProcessor();
        }
    }

    KeFatalError(KE_PANIC_NMI_HARDWARE_FAILURE);
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
    /* FLT_INVALID_OPERATION is our "magic number" to indicate we need to expand which floating
     * point fault happend. TODO: There is a bit more of handling (of other exceptions) we need to
     * do here too. */
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
    }

    /* Now build the exception record. */
    RtExceptionRecord ExceptionRecord;
    memset(&ExceptionRecord, 0, sizeof(RtExceptionRecord));
    ExceptionRecord.ExceptionCode = ExceptionCode;
    ExceptionRecord.ExceptionFlags = 0;
    ExceptionRecord.ExceptionRecord = NULL;
    ExceptionRecord.ExceptionAddress = (void *)InterruptFrame->Rip;
    ExceptionRecord.NumberOfParameters = 0;

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

    /* If we fail to dispatch, we're just falling back to panicking. */
    if (!RtDispatchException(&ExceptionRecord, &ContextRecord)) {
        KeFatalError(KE_PANIC_TRAP_NOT_HANDLED);
    }

    /* Otherwise, we gotta do the opposite now (rebuild our interrupt and exception frames using the
     * context record). */
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
    KeProcessor *Processor = HalGetCurrentProcessor();
    RtDList *HandlerList = &Processor->InterruptList[InterruptFrame->InterruptNumber];

    for (RtDList *ListHeader = HandlerList->Next; ListHeader != HandlerList;
         ListHeader = ListHeader->Next) {
        HalInterrupt *Interrupt = CONTAINING_RECORD(ListHeader, HalInterrupt, ListHeader);
        HalpSetIrql(Interrupt->Irql);
        __asm__ volatile("sti");
        KeAcquireSpinLockHighIrql(&Interrupt->Lock);
        Interrupt->Handler(InterruptFrame, Interrupt->HandlerContext);
        KeReleaseSpinLockHighIrql(&Interrupt->Lock);
        __asm__ volatile("cli");
        HalpSetIrql(InterruptFrame->Irql);
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
        else if (i == HAL_INT_DISPATCH_VECTOR) {
            Base = (uint64_t)HalpDispatchEntry;
        } else if (i == HAL_INT_TIMER_VECTOR) {
            Base = (uint64_t)HalpTimerEntry;
        }

        /* Default IRQ handlers for everything else. */
        else {
            Base = (uint64_t)&HalpDefaultInterruptHandlers[i - 32];
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
 *     This function attempts to enable the handling of the given interrupt.
 *
 * PARAMETERS:
 *     Interrupt - Target interrupt to be enabled.
 *
 * RETURN VALUE:
 *     0 if the interrupt couldn't be registered, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int HalEnableInterrupt(HalInterrupt *Interrupt) {
    /* We'll be assuming that if the interrupt is already enabled, the caller would rather receive a
     * "success" than a failure (as a previous call with the exact same interrupt object did
     * succeed). */
    if (Interrupt->Enabled) {
        return 1;
    }

    /* The IOAPIC uses the top 4 bits as the TPR (which is our IRQL); TPRs of 0 and 1 don't make
     * sense (they are traps/exceptions!), and we don't want anyone registering any interrupt
     * directly at dispatch level (just register a DPC instead). */
    if (Interrupt->Irql != (Interrupt->Vector >> 4) && Interrupt->Irql <= KE_IRQL_DISPATCH) {
        return 0;
    }

    /* Related to that, don't try using any reserved vector! */
    if (Interrupt->Vector == HAL_INT_TIMER_VECTOR) {
        return 0;
    }

    /* Other than that, we also are limited by our IDT (only 256 vectors). */
    if (Interrupt->Vector > 255) {
        return 0;
    }

    KeProcessor *Processor = HalGetCurrentProcessor();
    KeIrql OldIrql = KeAcquireSpinLock(&Processor->InterruptListLock);
    RtDList *Handlers = &Processor->InterruptList[Interrupt->Vector];

    if (Handlers->Next != Handlers) {
        /* We already have one or more interrupt handlers on this vector, validate if we are
         * compatible with the already installed handlers (one single vector can only be level or
         * edge triggered, not both). */
        if (CONTAINING_RECORD(Handlers->Next, HalInterrupt, ListHeader)->Type != Interrupt->Type) {
            KeReleaseSpinLock(&Processor->InterruptListLock, OldIrql);
            return 0;
        }
    }

    RtAppendDList(Handlers, &Interrupt->ListHeader);
    KeReleaseSpinLock(&Processor->InterruptListLock, OldIrql);
    Interrupt->Enabled = 1;

    return 1;
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
    KeProcessor *Processor = HalGetCurrentProcessor();
    KeIrql OldIrql = KeAcquireSpinLock(&Processor->InterruptListLock);
    RtUnlinkDList(&Interrupt->ListHeader);
    KeReleaseSpinLock(&Processor->InterruptListLock, OldIrql);
    Interrupt->Enabled = 0;
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
