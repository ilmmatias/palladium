/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <display.h>
#include <memory.h>
#include <string.h>
#include <x86/bios.h>

typedef struct __attribute__((packed)) {
    uint64_t Base;
    uint64_t Size;
    uint32_t Type;
    uint32_t ExtendedAttributes;
} E820Entry;

extern BiMemoryDescriptor Descriptors[BI_MAX_MEMORY_DESCRIPTORS];
extern int Count;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the memory map structures; After this, we're free to do memory
 *     allocations.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BiInitializeMemory(void) {
    BiosRegisters Registers;
    E820Entry Entry;

    memset(&Registers, 0, sizeof(BiosRegisters));
    Registers.Eax = 0xE820;
    Registers.Ecx = 24;
    Registers.Edx = 0x534D4150;
    Registers.Edi = (uint32_t)&Entry;

    /* Initial call will fail if we don't support E820h (is there even any amd64 motherboard that
       doesn't support it?) */
    BiosCall(0x15, &Registers);
    if (Registers.Eax != 0x534D4150 || !Registers.Ebx || (Registers.Eflags & 1)) {
        BmPrint("Could not get the system's memory map using the BIOS E820h function.\n"
                "You'll need to restart your device.\n");
        while (1)
            ;
    }

    while (Registers.Ebx && !(Registers.Eflags & 1)) {
        if (Entry.Size && (Registers.Ecx < 24 || (Entry.ExtendedAttributes & 1))) {
            BiAddMemoryDescriptor(
                Entry.Type == 1 ? BM_MD_FREE : BM_MD_HARDWARE, Entry.Base, Entry.Size);
        }

        Registers.Eax = 0xE820;
        Registers.Ecx = 24;
        BiosCall(0x15, &Registers);
    }
}
