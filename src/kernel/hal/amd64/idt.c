/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <amd64/halp.h>
#include <ki.h>
#include <mm.h>
#include <rt/except.h>
#include <stdio.h>
#include <vid.h>

typedef struct {
    RtSList ListHeader;
    void (*Handler)(HalRegisterState *);
} IdtHandler;

extern void EvpHandleEvents(HalRegisterState *);
extern uint64_t HalpInterruptHandlerTable[256];

extern KeSpinLock KiPanicLock;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Interrupt handler for the APIC; We redirect the interrupt to the correct place (or crash
 *     the system), followed by sending EOI to the APIC.
 *
 * PARAMETERS:
 *     State - I/O; Pointer into the stack to where the current register state was saved.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpInterruptHandler(HalRegisterState *State) {
    KeProcessor *Processor = (KeProcessor *)HalGetCurrentProcessor();
    KeIrql Irql = KeRaiseIrql(Processor->IdtIrqlSlots[State->InterruptNumber]);
    __asm__ volatile("sti");

    if (State->InterruptNumber < 32) {
        /* We don't care about the current IRQL, reset it to DISPATCH, or most functions we want
         * to use won't work. */
        HalpSetIrql(KE_IRQL_DISPATCH);

        /* This should just halt if someone else already panicked; No need for LockGuard, we're
         * not releasing this. */
        KeAcquireSpinLock(&KiPanicLock);

        /* Panics always halt everyone (the system isn't in a safe state anymore). */
        for (RtSList *ListHeader = HalpProcessorListHead.Next; ListHeader;
             ListHeader = ListHeader->Next) {
            KeProcessor *Processor = CONTAINING_RECORD(ListHeader, KeProcessor, ListHeader);
            Processor->EventStatus = KE_PANIC_EVENT;
            HalpNotifyProcessor(Processor, 0);
        }

        char ErrorMessage[256];
        snprintf(
            ErrorMessage,
            256,
            "Processor %u received exception %llu\n",
            Processor->ApicId,
            State->InterruptNumber);

        VidSetColor(VID_COLOR_PANIC);
        VidPutString("CANNOT SAFELY RECOVER OPERATION\n");
        VidPutString(ErrorMessage);

        RtContext Context;
        RtSaveContext(&Context);
        VidPutString("\nSTACK TRACE:\n");

        while (1) {
            KiDumpSymbol((void *)Context.Rip);

            if (Context.Rip < MM_PAGE_SIZE) {
                break;
            }

            uint64_t ImageBase = RtLookupImageBase(Context.Rip);
            RtVirtualUnwind(
                RT_UNW_FLAG_NHANDLER,
                ImageBase,
                Context.Rip,
                RtLookupFunctionEntry(ImageBase, Context.Rip),
                &Context,
                NULL,
                NULL);
        }

        while (1) {
            HalpStopProcessor();
        }
    }

    RtSList *ListHeader = Processor->IdtSlots[State->InterruptNumber - 32].ListHead.Next;
    while (ListHeader) {
        CONTAINING_RECORD(ListHeader, IdtHandler, ListHeader)->Handler(State);
        ListHeader = ListHeader->Next;
    }

    HalpSendEoi();
    __asm__ volatile("cli");
    KeLowerIrql(Irql);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function starts the interrupt handler setup process, telling the CPU we want to handle
 *     any incoming interrupts with HalpInterruptHandler.
 *
 * PARAMETERS:
 *     Processor - Pointer to the processor-specific structure.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpInitializeIdt(KeProcessor *Processor) {
    /* Interrupts remain disabled up until the Local APIC is configured (our interrupt handler
       is setup to send EOI to the APIC, not the PIC). */
    __asm__ volatile("cli");

    for (int i = 0; i < 256; i++) {
        Processor->IdtIrqlSlots[i] = KE_IRQL_MASK;
    }

    for (int i = 0; i < 256; i++) {
        Processor->IdtEntries[i].BaseLow = HalpInterruptHandlerTable[i];
        Processor->IdtEntries[i].Cs = 0x08;
        Processor->IdtEntries[i].Ist = 0;
        Processor->IdtEntries[i].Attributes = 0x8E;
        Processor->IdtEntries[i].BaseMid = HalpInterruptHandlerTable[i] >> 16;
        Processor->IdtEntries[i].BaseHigh = HalpInterruptHandlerTable[i] >> 32;
        Processor->IdtEntries[i].Reserved = 0;
    }

    for (int i = 0; i < 224; i++) {
        Processor->IdtSlots[i].Usage = 0;
        Processor->IdtSlots[i].ListHead.Next = NULL;
    }

    Processor->IdtDescriptor.Limit = sizeof(Processor->IdtEntries) - 1;
    Processor->IdtDescriptor.Base = (uint64_t)Processor->IdtEntries;
    __asm__ volatile("lidt %0" : : "m"(Processor->IdtDescriptor));

    /* Register the DPC/event handler. */
    HalInstallInterruptHandlerAt(0x40, EvpHandleEvents, KE_IRQL_DISPATCH);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function installs an interrupt handler at the given vector.
 *
 * PARAMETERS:
 *     Vector - Where to install the handler.
 *     Handler - Which function will handle it.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int HalInstallInterruptHandlerAt(
    uint8_t Vector,
    void (*Handler)(HalRegisterState *),
    KeIrql TargetIrql) {
    IdtHandler *Entry = MmAllocatePool(sizeof(IdtHandler), "Apic");
    if (!Entry) {
        return 0;
    }

    KeProcessor *Processor = (KeProcessor *)HalGetCurrentProcessor();
    Processor->IdtSlots[Vector - 32].Usage++;
    Processor->IdtIrqlSlots[Vector] = TargetIrql;
    Entry->Handler = Handler;
    RtPushSList(&Processor->IdtSlots[Vector - 32].ListHead, &Entry->ListHeader);

    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function allocates the next least used IRQ, and installs the given interrupt handler
 *     into it.
 *
 * PARAMETERS:
 *     Handler - Which function will handle it.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
uint8_t HalInstallInterruptHandler(void (*Handler)(HalRegisterState *), KeIrql TargetIrql) {
    KeProcessor *Processor = (KeProcessor *)HalGetCurrentProcessor();
    uint32_t SmallestUsage = UINT32_MAX;
    uint8_t Index = 0;

    for (uint8_t i = 0; i < 224; i++) {
        if (Processor->IdtSlots[i].Usage < SmallestUsage) {
            SmallestUsage = Processor->IdtSlots[i].Usage;
            Index = i;
        }
    }

    return HalInstallInterruptHandlerAt(Index + 32, Handler, TargetIrql) ? Index + 32 : -1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function enters a critical code path (no interrupts allowed).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Arch-specific context; Either a 0 (interrupts were disabled) or >0 (interrupts were enabled)
 *     in our case.
 *-----------------------------------------------------------------------------------------------*/
void *HalpEnterCriticalSection(void) {
    uint64_t Flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(Flags));
    return (void *)(Flags & 0x200);
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
    if (Context) {
        __asm__ volatile("sti");
    }
}
