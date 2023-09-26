/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries the execute the given opcode as a Data Object opcode.
 *
 * PARAMETERS:
 *     State - AML state containing the current scope.
 *     Opcode - Opcode to be executed; The high bits contain the extended opcode number (when the
 *              lower opcode is 0x5B).
 *     Value - Output; Where to store the resulting Value.
 *
 * RETURN VALUE:
 *     Positive number on success, negative on "this isn't a data object", 0 on failure.
 *-----------------------------------------------------------------------------------------------*/
int AcpipExecuteDataObjOpcode(AcpipState *State, uint16_t Opcode, AcpiValue *Value) {
    uint32_t Start = State->Scope->RemainingLength;

    switch (Opcode) {
        /* ZeroOp */
        case 0x00: {
            Value->Type = ACPI_INTEGER;
            Value->References = 1;
            Value->Integer = 0;
            break;
        }

        /* OneOp */
        case 0x01: {
            Value->Type = ACPI_INTEGER;
            Value->References = 1;
            Value->Integer = 1;
            break;
        }

        /* ByteConst := BytePrefix ByteData */
        case 0x0A: {
            Value->Type = ACPI_INTEGER;
            Value->References = 1;

            if (!AcpipReadByte(State, (uint8_t *)&Value->Integer)) {
                return 0;
            }

            break;
        }

        /* WordConst := WordPrefix WordData */
        case 0x0B: {
            Value->Type = ACPI_INTEGER;
            Value->References = 1;

            if (!AcpipReadWord(State, (uint16_t *)&Value->Integer)) {
                return 0;
            }

            break;
        }

        /* DWordConst := DWordPrefix DWordData */
        case 0x0C: {
            Value->Type = ACPI_INTEGER;
            Value->References = 1;

            if (!AcpipReadDWord(State, (uint32_t *)&Value->Integer)) {
                return 0;
            }

            break;
        }

        /* String := StringPrefix AsciiCharList NullChar */
        case 0x0D: {
            Value->Type = ACPI_STRING;
            Value->References = 1;

            size_t StringSize = 0;
            while (StringSize < State->Scope->RemainingLength && State->Scope->Code[StringSize]) {
                StringSize++;
            }

            if (++StringSize > State->Scope->RemainingLength) {
                return 0;
            }

            Value->String = malloc(sizeof(AcpiString) + StringSize);
            if (!Value->String) {
                return 0;
            }

            Value->String->References = 1;
            memcpy(Value->String->Data, State->Scope->Code, StringSize);
            State->Scope->Code += StringSize;
            State->Scope->RemainingLength -= StringSize;

            break;
        }

        /* QWordConst := QWordPrefix QWordData */
        case 0x0E: {
            Value->Type = ACPI_INTEGER;
            Value->References = 1;

            if (!AcpipReadQWord(State, &Value->Integer)) {
                return 0;
            }

            break;
        }

        /* DefBuffer := BufferOp PkgLength BufferSize ByteList */
        case 0x11: {
            Value->Type = ACPI_BUFFER;
            Value->References = 1;

            uint32_t PkgLength;
            uint64_t BufferSize;
            if (!AcpipReadPkgLength(State, &PkgLength)) {
                return 0;
            } else if (!AcpipExecuteInteger(State, &BufferSize)) {
                return 0;
            }

            uint32_t LengthSoFar = Start - State->Scope->RemainingLength;
            if (LengthSoFar > PkgLength ||
                PkgLength - LengthSoFar > State->Scope->RemainingLength ||
                PkgLength - LengthSoFar > BufferSize) {
                free(Value->Buffer);
                return 0;
            }

            Value->Buffer = calloc(1, sizeof(AcpiBuffer) + BufferSize);
            if (!Value->Buffer) {
                return 0;
            }

            Value->Buffer->References = 1;
            Value->Buffer->Size = BufferSize;

            for (uint32_t i = 0; i < PkgLength - LengthSoFar; i++) {
                Value->Buffer->Data[i] = *(State->Scope->Code++);
                State->Scope->RemainingLength--;
            }

            break;
        }

        /* DefPackage := PackageOp PkgLength NumElements PackageElementList
           DefVarPackage := VarPackageOp PkgLength VarNumElements PackageElementList */
        case 0x12:
        case 0x13: {
            Value->Type = ACPI_PACKAGE;
            Value->References = 1;

            uint32_t PkgLength;
            if (!AcpipReadPkgLength(State, &PkgLength)) {
                return 0;
            }

            /* The difference between DefPackage and DefVarPackage is NumElements vs
               VarNumElements; NumElements is always an 8-bit constant, which VarNumElements
               is a term arg (ExecuteInteger). */
            uint64_t Size = 0;
            if (Opcode == 0x13) {
                if (!AcpipExecuteInteger(State, &Size)) {
                    return 0;
                }
            } else {
                if (!AcpipReadByte(State, (uint8_t *)&Size)) {
                    return 0;
                }
            }

            uint32_t LengthSoFar = Start - State->Scope->RemainingLength;
            if (LengthSoFar > PkgLength ||
                PkgLength - LengthSoFar > State->Scope->RemainingLength) {
                return 0;
            }

            PkgLength -= LengthSoFar;
            Value->Package = calloc(1, sizeof(AcpiPackage) + Size * sizeof(AcpiPackageElement));
            if (!Value->Package) {
                return 0;
            }

            Value->Package->References = 1;
            Value->Package->Size = Size;

            uint8_t i = 0;
            while (PkgLength) {
                if (i >= Value->Package->Size) {
                    AcpiRemoveReference(Value, 0);
                    return 0;
                }

                uint32_t Start = State->Scope->RemainingLength;
                uint8_t Opcode = *State->Scope->Code;

                /* Each PackageElement should always be either a DataRefObject (which we just call
                   ExecuteOpcode to handle), or a NameString. */
                if (Opcode != '\\' && Opcode != '^' && Opcode != 0x2E && Opcode != 0x2F &&
                    !isupper(Opcode) && Opcode != '_') {
                    Value->Package->Data[i].Type = 1;
                    if (!AcpipExecuteOpcode(State, &Value->Package->Data[i].Value, 1)) {
                        Value->Package->Size = i ? i - 1 : 0;
                        AcpiRemoveReference(Value, 0);
                        return 0;
                    }
                } else {
                    AcpipName Name;
                    if (!AcpipReadName(State, &Name)) {
                        Value->Package->Size = i ? i - 1 : 0;
                        AcpiRemoveReference(Value, 0);
                        return 0;
                    }
                }

                i++;
                if (Start - State->Scope->RemainingLength > PkgLength) {
                    Value->Package->Size = i - 1;
                    AcpiRemoveReference(Value, 0);
                    return 0;
                }

                PkgLength -= Start - State->Scope->RemainingLength;
            }

            break;
        }

        /* OnesOp */
        case 0xFF: {
            Value->Type = ACPI_INTEGER;
            Value->References = 1;
            Value->Integer = UINT64_MAX;
            break;
        }

        /* RevisionOp */
        case 0x305B: {
            Value->Type = ACPI_INTEGER;
            Value->References = 1;
            Value->Integer = ACPI_REVISION;
            break;
        }

        default:
            return -1;
    }

    return 1;
}
