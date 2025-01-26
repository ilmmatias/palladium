/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <amd64/halp.h>

extern struct __attribute__((packed)) {
    char PushOp[2];
    char JumpOp[5];
} HalpDefaultInterruptHandlers[224];

extern void HalpDefaultTrapEntryWithoutErrorCode(void);
extern void HalpDefaultTrapEntryWithErrorCode(void);
extern void HalpPageFaultTrapEntry(void);
extern void HalpNmiTrapEntry(void);
extern void HalpDispatchEntry(void);
extern void HalpTimerEntry(void);

static void (*TrapEntries[32])(void) = {
    HalpDefaultTrapEntryWithoutErrorCode,
    HalpDefaultTrapEntryWithoutErrorCode,
    HalpNmiTrapEntry,
    HalpDefaultTrapEntryWithoutErrorCode,
    HalpDefaultTrapEntryWithoutErrorCode,
    HalpDefaultTrapEntryWithoutErrorCode,
    HalpDefaultTrapEntryWithoutErrorCode,
    HalpDefaultTrapEntryWithoutErrorCode,
    HalpDefaultTrapEntryWithErrorCode,
    HalpDefaultTrapEntryWithoutErrorCode,
    HalpDefaultTrapEntryWithErrorCode,
    HalpDefaultTrapEntryWithErrorCode,
    HalpDefaultTrapEntryWithErrorCode,
    HalpDefaultTrapEntryWithErrorCode,
    HalpPageFaultTrapEntry,
    HalpDefaultTrapEntryWithoutErrorCode,
    HalpDefaultTrapEntryWithoutErrorCode,
    HalpDefaultTrapEntryWithErrorCode,
    HalpDefaultTrapEntryWithoutErrorCode,
    HalpDefaultTrapEntryWithoutErrorCode,
    HalpDefaultTrapEntryWithoutErrorCode,
    HalpDefaultTrapEntryWithErrorCode,
    HalpDefaultTrapEntryWithoutErrorCode,
    HalpDefaultTrapEntryWithoutErrorCode,
    HalpDefaultTrapEntryWithoutErrorCode,
    HalpDefaultTrapEntryWithoutErrorCode,
    HalpDefaultTrapEntryWithoutErrorCode,
    HalpDefaultTrapEntryWithoutErrorCode,
    HalpDefaultTrapEntryWithoutErrorCode,
    HalpDefaultTrapEntryWithErrorCode,
    HalpDefaultTrapEntryWithErrorCode,
    HalpDefaultTrapEntryWithoutErrorCode,
};

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

        /* Exception handlers. */
        if (i < 32) {
            Base = (uint64_t)TrapEntries[i];
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

        InitializeEntry(Processor, i, Base, DESCR_SEG_KCODE, 0, IDT_TYPE_INT, DESCR_DPL_KERNEL);
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
