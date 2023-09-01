/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpi.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

AcpiObject *AcpipExecuteTermList(AcpipState *State) {
    while (1) {
        if (!State->RemainingLength) {
            /* Backtrack into the previous scope, or end if we're already in the top-most
               scope. */
            if (State->Parent) {
                AcpipState *Parent = State->Parent;

                if (!State->InMethod) {
                    Parent->Code = Parent->Code;
                    Parent->RemainingLength -= State->Length;
                }

                free(State);
                State = Parent;
                continue;
            }

            break;
        }

        uint8_t Opcode = *(State->Code++);
        State->RemainingLength--;

        uint8_t ExtOpcode = 0;
        if (Opcode == 0x80 && !AcpipReadByte(State, &ExtOpcode)) {
            return NULL;
        }

        switch (Opcode | ((uint16_t)ExtOpcode << 8)) {
            default:
                printf("unimplemented opcode: %#hx\n", Opcode | ((uint32_t)ExtOpcode << 8));
                while (1)
                    ;
        }
    }

    AcpiObject *Object = malloc(sizeof(AcpiObject));
    if (Object) {
        Object->Type = ACPI_OBJECT_INTEGER;
        Object->Value.Integer = 0;
    }

    return Object;
}
