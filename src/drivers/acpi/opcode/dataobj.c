/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <acpip.h>
#include <ctype.h>
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
    uint32_t Start = State->Opcode->Start;

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
        case 0x0A:
        case 0x0B:
        case 0x0C:
        case 0x0E: {
            Value->Type = ACPI_INTEGER;
            Value->References = 1;
            Value->Integer = State->Opcode->FixedArguments[0].Integer;
            break;
        }

        /* String := StringPrefix AsciiCharList NullChar */
        case 0x0D: {
            Value->Type = ACPI_STRING;
            Value->References = 1;
            Value->String = State->Opcode->FixedArguments[0].String;
            break;
        }

        /* DefBuffer := BufferOp PkgLength BufferSize ByteList */
        case 0x11: {
            Value->Type = ACPI_BUFFER;
            Value->References = 1;

            uint32_t PkgLength = State->Opcode->PkgLength;
            uint64_t BufferSize = State->Opcode->FixedArguments[0].TermArg.Integer;
            uint32_t LengthSoFar = Start - State->Scope->RemainingLength;
            if (LengthSoFar > PkgLength ||
                PkgLength - LengthSoFar > State->Scope->RemainingLength ||
                PkgLength - LengthSoFar > BufferSize) {
                AcpipFreeBlock(Value->Buffer);
                return 0;
            }

            Value->Buffer = AcpipAllocateZeroBlock(1, sizeof(AcpiBuffer) + BufferSize);
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

            /* The difference between DefPackage and DefVarPackage is NumElements vs
               VarNumElements; NumElements is always an 8-bit constant, which VarNumElements
               is a term arg. */
            uint64_t Size = Opcode == 0x13 ? State->Opcode->FixedArguments[0].TermArg.Integer
                                           : State->Opcode->FixedArguments[0].Integer;

            uint32_t PkgLength = State->Opcode->PkgLength;
            uint32_t LengthSoFar = Start - State->Scope->RemainingLength;
            if (LengthSoFar > PkgLength ||
                PkgLength - LengthSoFar > State->Scope->RemainingLength) {
                return 0;
            }

            PkgLength -= LengthSoFar;
            Value->Package =
                AcpipAllocateZeroBlock(1, sizeof(AcpiPackage) + Size * sizeof(AcpiPackageElement));
            if (!Value->Package) {
                return 0;
            }

            for (uint64_t i = 0; i < Size; i++) {
                Value->Package->Data[i].Type = 1;
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

                /* Each PackageElement should always be either a NameString, or a DataRefObject. */
                if (Opcode == '\\' || Opcode == '^' || Opcode == 0x2E || Opcode == 0x2F ||
                    isupper(Opcode) || Opcode == '_') {
                    Value->Package->Data[i].Type = 0;
                    if (!AcpipReadName(State, &Value->Package->Data[i].Name)) {
                        Value->Package->Size = i;
                        AcpiRemoveReference(Value, 0);
                        return 0;
                    }
                } else {
                    Value->Package->Data[i].Type = 1;
                    if (!AcpipPrepareExecuteOpcode(State) ||
                        !AcpipExecuteOpcode(State, &Value->Package->Data[i].Value)) {
                        Value->Package->Size = i;
                        AcpiRemoveReference(Value, 0);
                        return 0;
                    }
                }

                i++;
                if (Start - State->Scope->RemainingLength > PkgLength) {
                    Value->Package->Size = i;
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
