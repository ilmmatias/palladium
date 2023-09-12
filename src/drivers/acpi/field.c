/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int AcpipReadFieldList(AcpipState *State, AcpiValue *Base, uint32_t Start, uint32_t Length) {
    uint32_t LengthSoFar = Start - State->Scope->RemainingLength;
    if (LengthSoFar >= Length || Length - LengthSoFar > State->Scope->RemainingLength) {
        return 0;
    }

    /* Last part of a Field def, should always be `... FieldFlags FieldList`. */
    uint8_t AccessType = *(State->Scope->Code++);
    uint8_t AccessAttrib = 0;
    uint8_t AccessLength = 0;
    uint32_t Offset = 0;

    Length -= LengthSoFar + 1;
    State->Scope->RemainingLength--;

    while (Length) {
        uint32_t Start = State->Scope->RemainingLength;

        switch (*State->Scope->Code) {
            /* ReservedField := 0x00 PkgLength */
            case 0x00: {
                State->Scope->RemainingLength--;
                State->Scope->Code++;

                uint32_t Length;
                if (!AcpipReadPkgLength(State, &Length)) {
                    return 0;
                }

                break;
            }

            /* AccessField := 0x01 AccessType AccessAtrib
               ExtendedAccessField := 0x03 AccessType AccessAttrib AccessLength */
            case 0x01:
            case 0x03:
                State->Scope->RemainingLength -= 3;
                State->Scope->Code++;

                AccessType = *(State->Scope->Code++);
                AccessAttrib = *(State->Scope->Code++);

                if (*(State->Scope->Code - 3) == 0x03) {
                    AccessLength = *(State->Scope->Code++);
                    State->Scope->RemainingLength--;
                }

                break;

            /* ConnectField */
            case 0x02:
                printf("ConnectField (unimplemented)\n");
                while (1)
                    ;

            /* NamedField := NameSeg PkgLength */
            default: {
                /* Just a single NameSeg is equivalent to a normal NamePath, so we can use
                   ReadName). */
                AcpipName *Name = AcpipReadName(State);
                if (!Name) {
                    return 0;
                }

                uint32_t Length;
                if (!AcpipReadPkgLength(State, &Length)) {
                    free(Name);
                    return 0;
                }

                AcpiValue Value;
                memcpy(&Value, Base, sizeof(AcpiValue));
                Value.Field.AccessType = AccessType;
                Value.Field.AccessAttrib = AccessAttrib;
                Value.Field.AccessLength = AccessLength;
                Value.Field.Offset = Offset;
                Value.Field.Length = Length;
                Offset += Length;

                if (!AcpipCreateObject(Name, &Value)) {
                    free(Name);
                    return 0;
                }

                break;
            }
        }

        if (Start - State->Scope->RemainingLength > Length) {
            return 0;
        }

        Length -= Start - State->Scope->RemainingLength;
    }

    return 1;
}
