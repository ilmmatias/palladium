/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpi.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

AcpiValue *AcpipExecuteTermList(AcpipState *State) {
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
        if (Opcode == 0x5B && !AcpipReadByte(State, &ExtOpcode)) {
            return NULL;
        }

        switch (Opcode | ((uint16_t)ExtOpcode << 8)) {
            /* DefScope := ScopeOp PkgLength NameString TermList */
            case 0x10: {
                uint32_t Length;
                if (!AcpipReadPkgLength(State, &Length)) {
                    return NULL;
                }

                char *Name;
                uint8_t NameSegs;
                if (!AcpipReadNameString(State, &Name, &NameSegs)) {
                    return NULL;
                }

                AcpipState *Scope = AcpipEnterSubScope(State, Name, NameSegs, Length);
                if (!Scope) {
                    free(Name);
                    return NULL;
                }

                State = Scope;
                break;
            }

            /* DefOpRegion := OpRegionOp NameString RegionSpace RegionOffset RegionLen */
            case 0x805B: {
                char *Name;
                uint8_t NameSegs;
                if (!AcpipReadNameString(State, &Name, &NameSegs)) {
                    return NULL;
                }

                uint8_t RegionSpace;
                if (!AcpipReadByte(State, &RegionSpace)) {
                    free(Name);
                    return NULL;
                }

                AcpiValue *RegionOffset = AcpipExecuteTermArg(State);
                if (!RegionOffset) {
                    free(Name);
                    return NULL;
                }

                AcpiValue *RegionLen = AcpipExecuteTermArg(State);
                if (!RegionLen) {
                    free(Name);
                    return NULL;
                }

                break;
            }

            /* DefField := FieldOp PkgLength NameString FieldFlags FieldList */
            case 0x815B: {
                uint32_t Length;
                if (!AcpipReadPkgLength(State, &Length)) {
                    return NULL;
                }

                char *Name;
                uint8_t NameSegs;
                if (!AcpipReadNameString(State, &Name, &NameSegs)) {
                    return NULL;
                }

                printf(
                    "unimplemented opcode: %#hx (DefField)\n", Opcode | ((uint32_t)ExtOpcode << 8));

                uint8_t FieldFlags;
                if (!AcpipReadFieldList(State, &FieldFlags)) {
                    free(Name);
                    return NULL;
                }

                break;
            }

            default:
                printf("unimplemented opcode: %#hx\n", Opcode | ((uint32_t)ExtOpcode << 8));
                return NULL;
        }
    }

    AcpiValue *Value = malloc(sizeof(AcpiValue));
    if (Value) {
        Value->Type = ACPI_VALUE_INTEGER;
        Value->Integer = 0;
    }

    return Value;
}
