/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <amd64/halp.h>
#include <ke.h>
#include <mm.h>
#include <rt.h>
#include <vid.h>

typedef struct {
    RtSList ListHeader;
    void (*Handler)(HalRegisterState *);
} IdtHandler;

extern void KiHandleEvent(void *);
extern uint64_t HalpInterruptHandlerTable[256];

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
    if (State->InterruptNumber < 32) {
        VidPrint(
            KE_MESSAGE_ERROR,
            "Kernel HAL",
            "processor %u received exception %llu\n",
            HalpGetCurrentProcessor()->ApicId,
            State->InterruptNumber);
        while (1)
            ;
    }

    RtSList *ListHeader =
        HalpGetCurrentProcessor()->IdtSlots[State->InterruptNumber - 32].ListHead.Next;
    while (ListHeader) {
        CONTAINING_RECORD(ListHeader, IdtHandler, ListHeader)->Handler(State);
        ListHeader = ListHeader->Next;
    }

    HalpSendEoi();
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
void HalpInitializeIdt(HalpProcessor *Processor) {
    /* Interrupts remain disabled up until the Local APIC is configured (our interrupt handler
       is setup to send EOI to the APIC, not the PIC). */
    __asm__ volatile("cli");

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
    __asm__ volatile("lidt %0" :: "m"(Processor->IdtDescriptor));

    /* Register the DPC/event handler. */
    HalInstallInterruptHandlerAt(0xDE, (void (*)(HalRegisterState *))KiHandleEvent);
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
int HalInstallInterruptHandlerAt(uint8_t Vector, void (*Handler)(HalRegisterState *)) {
    IdtHandler *Entry = MmAllocatePool(sizeof(IdtHandler), "Apic");
    if (!Entry) {
        return 0;
    }

    HalpProcessor *Processor = HalpGetCurrentProcessor();
    Processor->IdtSlots[Vector].Usage++;
    Entry->Handler = Handler;
    RtPushSList(&Processor->IdtSlots[Vector].ListHead, &Entry->ListHeader);

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
uint8_t HalInstallInterruptHandler(void (*Handler)(HalRegisterState *)) {
    HalpProcessor *Processor = HalpGetCurrentProcessor();
    uint32_t SmallestUsage = UINT32_MAX;
    uint8_t Index = 0;

    for (uint8_t i = 0; i < 224; i++) {
        if (Processor->IdtSlots[i].Usage < SmallestUsage) {
            SmallestUsage = Processor->IdtSlots[i].Usage;
            Index = i;
        }
    }

    return HalInstallInterruptHandlerAt(Index, Handler) ? Index : -1;
}
