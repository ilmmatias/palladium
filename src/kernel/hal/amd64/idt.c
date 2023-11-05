/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <amd64/halp.h>
#include <amd64/regs.h>
#include <ke.h>
#include <vid.h>

typedef struct __attribute__((packed)) {
    uint16_t BaseLow;
    uint16_t Cs;
    uint8_t Ist;
    uint8_t Attributes;
    uint16_t BaseMid;
    uint32_t BaseHigh;
    uint32_t Reserved;
} IdtEntry;

typedef struct __attribute__((packed)) {
    uint16_t Limit;
    uint64_t Base;
} IdtDescriptor;

extern uint64_t HalpInterruptHandlerTable[256];

static __attribute__((aligned(0x10))) IdtEntry Entries[256];
static IdtDescriptor Descriptor;

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
void HalpInterruptHandler(RegisterState *State) {
    VidPrint(KE_MESSAGE_INFO, "Kernel", "received interrupt %llu\n", State->InterruptNumber);

    if (State->InterruptNumber < 32) {
        while (1)
            ;
    }

    HalpSendEoi();
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function starts the interrupt handler setup process, telling the CPU we want to handle
 *     any incoming interrupts with HalpInterruptHandler.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpInitializeIdt(void) {
    /* Interrupts remain disabled up until the Local APIC is configured (our interrupt handler
       is setup to send EOI to the APIC, not the PIC). */
    __asm__ volatile("cli");

    for (int i = 0; i < 256; i++) {
        Entries[i].BaseLow = HalpInterruptHandlerTable[i];
        Entries[i].Cs = 0x08;
        Entries[i].Ist = 0;
        Entries[i].Attributes = 0x8E;
        Entries[i].BaseMid = HalpInterruptHandlerTable[i] >> 16;
        Entries[i].BaseHigh = HalpInterruptHandlerTable[i] >> 32;
        Entries[i].Reserved = 0;
    }

    Descriptor.Limit = sizeof(Entries) - 1;
    Descriptor.Base = (uint64_t)Entries;
    __asm__ volatile("lidt %0" :: "m"(Descriptor));
}
