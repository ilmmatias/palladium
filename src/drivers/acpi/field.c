/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static AcpiFieldElement *CreateElement(AcpiFieldElement **Root) {
    AcpiFieldElement *Parent = *Root;
    while (Parent && Parent->Next) {
        Parent = Parent->Next;
    }

    AcpiFieldElement *Element = calloc(1, sizeof(AcpiFieldElement));
    if (!Element) {
        return NULL;
    }

    if (Parent) {
        Parent->Next = Element;
    } else {
        *Root = Element;
    }

    return Element;
}

void AcpipFreeFieldList(AcpiFieldElement *Root) {
    while (Root) {
        AcpiFieldElement *Next = Root->Next;
        free(Root);
        Root = Next;
    }
}

int AcpipReadFieldList(
    AcpipState *State,
    uint32_t Start,
    uint32_t Length,
    uint8_t *FieldFlags,
    AcpiFieldElement **Fields) {
    uint32_t LengthSoFar = Start - State->Scope->RemainingLength;
    if (LengthSoFar >= Length || Length - LengthSoFar > State->Scope->RemainingLength) {
        return 0;
    }

    /* Last part of a Field def, should always be `... FieldFlags FieldList`. */
    *FieldFlags = *(State->Scope->Code++);
    Length -= LengthSoFar + 1;
    State->Scope->RemainingLength--;

    while (Length) {
        uint32_t Start = State->Scope->RemainingLength;
        AcpiFieldElement *Element = CreateElement(Fields);

        if (!Element) {
            AcpipFreeFieldList(*Fields);
            return 0;
        }

        switch (*State->Scope->Code) {
            /* ReservedField := 0x00 PkgLength */
            case 0x00:
                State->Scope->RemainingLength--;
                State->Scope->Code++;

                Element->Type = ACPI_RESERVED_FIELD;
                if (!AcpipReadPkgLength(State, &Element->Reserved.Length)) {
                    AcpipFreeFieldList(*Fields);
                    return 0;
                }

                break;

            /* AccessField := 0x01 AccessType AccessAtrib
               ExtendedAccessField := 0x03 AccessType AccessAttrib AccessLength */
            case 0x01:
            case 0x03:
                State->Scope->RemainingLength -= 3;
                State->Scope->Code++;

                Element->Type = ACPI_ACCESS_FIELD;
                Element->Access.Type = *(State->Scope->Code++);
                Element->Access.Attrib = *(State->Scope->Code++);

                if (*(State->Scope->Code - 1) == 0x03) {
                    Element->Access.Length = *(State->Scope->Code++);
                    State->Scope->RemainingLength--;
                }

                break;

            /* ConnectField */
            case 0x02:
                printf("ConnectField (unimplemented)\n");
                while (1)
                    ;

            /* NamedField := NameSeg PkgLength */
            default:
                State->Scope->RemainingLength -= 4;
                Element->Type = ACPI_NAMED_FIELD;
                memcpy(Element->Named.Name, State->Scope->Code, 4);

                State->Scope->Code += 4;
                if (!AcpipReadPkgLength(State, &Element->Named.Length)) {
                    AcpipFreeFieldList(*Fields);
                    return 0;
                }

                break;
        }

        if (Start - State->Scope->RemainingLength > Length) {
            AcpipFreeFieldList(*Fields);
            return 0;
        }

        Length -= Start - State->Scope->RemainingLength;
    }

    return 1;
}
