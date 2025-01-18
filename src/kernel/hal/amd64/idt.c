/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
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

extern void EvpHandleClock(HalRegisterState *);
extern void EvpHandleEvents(HalRegisterState *);

extern uint64_t HalpInterruptHandlerTable[256];

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles non-maskable interrupts, such as a hardware/memory failure, or a
 *     watchdog timer.
 *
 * PARAMETERS:
 *     State - I/O; Pointer into the stack to where the current register state was saved.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void HandleNmi(HalRegisterState *) {
    KeProcessor *Processor = (KeProcessor *)HalGetCurrentProcessor();

    /* Just halt execution if we got here from HalpFreezeProcessor. */
    if (Processor->EventStatus == KE_EVENT_FREEZE) {
        while (1) {
            HalpStopProcessor();
        }
    }

    /* Or panic if we didn't (hardware failure probably). */
    KeFatalError(KE_PANIC_NMI_HARDWARE_FAILURE);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles page faults/invalid memory accesses.
 *
 * PARAMETERS:
 *     State - I/O; Pointer into the stack to where the current register state was saved.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void HandlePageFault(HalRegisterState *) {
    KeFatalError(KE_PANIC_PAGE_FAULT_NOT_HANDLED);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles system exceptions and traps.
 *
 * PARAMETERS:
 *     State - I/O; Pointer into the stack to where the current register state was saved.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void HandleTrap(HalRegisterState *) {
    KeFatalError(KE_PANIC_TRAP_NOT_HANDLED);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles system service calls, like IRQs and IPIs.
 *
 * PARAMETERS:
 *     State - I/O; Pointer into the stack to where the current register state was saved.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void HandleService(HalRegisterState *State) {
    KeProcessor *Processor = (KeProcessor *)HalGetCurrentProcessor();
    KeIrql Irql = KeRaiseIrql(Processor->IdtIrqlSlots[State->InterruptNumber]);
    __asm__ volatile("sti");

    RtSList *ListHeader = Processor->IdtSlots[State->InterruptNumber - 32].ListHead.Next;
    if (ListHeader) {
        while (ListHeader) {
            CONTAINING_RECORD(ListHeader, IdtHandler, ListHeader)->Handler(State);
            ListHeader = ListHeader->Next;
        }
    } else {
        KeFatalError(KE_PANIC_SYSTEM_SERVICE_NOT_HANDLED);
    }

    HalpSendEoi();
    __asm__ volatile("cli");
    KeLowerIrql(Irql);
}

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
    if (State->InterruptNumber == 2) {
        HandleNmi(State);
    } else if (State->InterruptNumber == 14) {
        HandlePageFault(State);
    } else if (State->InterruptNumber < 32) {
        HandleTrap(State);
    } else {
        HandleService(State);
    }
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

    /* Register all our event handlers. */
    HalInstallInterruptHandlerAt(0x40, EvpHandleClock, KE_IRQL_CLOCK);
    HalInstallInterruptHandlerAt(0x41, EvpHandleEvents, KE_IRQL_DISPATCH);
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
