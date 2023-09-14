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
            Value->Integer = 0;
            break;
        }

        /* OneOp */
        case 0x01: {
            Value->Type = ACPI_INTEGER;
            Value->Integer = 1;
            break;
        }

        /* ByteConst := BytePrefix ByteData */
        case 0x0A: {
            Value->Type = ACPI_INTEGER;

            if (!AcpipReadByte(State, (uint8_t *)&Value->Integer)) {
                return 0;
            }

            break;
        }

        /* WordConst := WordPrefix WordData */
        case 0x0B: {
            Value->Type = ACPI_INTEGER;

            if (!AcpipReadWord(State, (uint16_t *)&Value->Integer)) {
                return 0;
            }

            break;
        }

        /* DWordConst := DWordPrefix DWordData */
        case 0x0C: {
            Value->Type = ACPI_INTEGER;

            if (!AcpipReadDWord(State, (uint32_t *)&Value->Integer)) {
                return 0;
            }

            break;
        }

        /* String := StringPrefix AsciiCharList NullChar */
        case 0x0D: {
            Value->Type = ACPI_STRING;

            size_t StringSize = 0;
            while (StringSize < State->Scope->RemainingLength && State->Scope->Code[StringSize]) {
                StringSize++;
            }

            if (StringSize > State->Scope->RemainingLength) {
                return 0;
            }

            Value->String = malloc(StringSize);
            if (!Value->String) {
                return 0;
            }

            memcpy(Value->String, State->Scope->Code, StringSize);
            State->Scope->Code += StringSize + 1;
            State->Scope->RemainingLength -= StringSize + 1;

            break;
        }

        /* QWordConst := QWordPrefix QWordData */
        case 0x0E: {
            Value->Type = ACPI_INTEGER;

            if (!AcpipReadQWord(State, &Value->Integer)) {
                return 0;
            }

            break;
        }

        /* DefBuffer := BufferOp PkgLength BufferSize ByteList */
        case 0x11: {
            Value->Type = ACPI_BUFFER;

            uint32_t PkgLength;
            if (!AcpipReadPkgLength(State, &PkgLength)) {
                return 0;
            } else if (!AcpipExecuteInteger(State, &Value->Buffer.Size)) {
                return 0;
            }

            Value->Buffer.Data = calloc(1, Value->Buffer.Size);
            if (!Value->Buffer.Data) {
                return 0;
            }

            uint32_t LengthSoFar = Start - State->Scope->RemainingLength;
            if (LengthSoFar > PkgLength ||
                PkgLength - LengthSoFar > State->Scope->RemainingLength ||
                PkgLength - LengthSoFar > Value->Buffer.Size) {
                free(Value->Buffer.Data);
                return 0;
            }

            for (uint32_t i = 0; i < PkgLength - LengthSoFar; i++) {
                Value->Buffer.Data[i] = *(State->Scope->Code++);
                State->Scope->RemainingLength--;
            }

            break;
        }

        /* DefPackage := PackageOp PkgLength NumElements PackageElementList
           DefVarPackage := VarPackageOp PkgLength VarNumElements PackageElementList */
        case 0x12:
        case 0x13: {
            Value->Type = ACPI_PACKAGE;

            uint32_t PkgLength;
            if (!AcpipReadPkgLength(State, &PkgLength)) {
                return 0;
            }

            /* The difference between DefPackage and DefVarPackage is NumElements vs
               VarNumElements; NumElements is always an 8-bit constant, which VarNumElements
               is a term arg (ExecuteInteger). */
            if (Opcode == 0x13) {
                if (!AcpipExecuteInteger(State, &Value->Package.Size)) {
                    return 0;
                }
            } else {
                if (!AcpipReadByte(State, (uint8_t *)&Value->Package.Size)) {
                    return 0;
                }
            }

            uint32_t LengthSoFar = Start - State->Scope->RemainingLength;
            if (LengthSoFar >= PkgLength ||
                PkgLength - LengthSoFar > State->Scope->RemainingLength) {
                return 0;
            }

            PkgLength -= LengthSoFar;
            Value->Package.Data = calloc(Value->Package.Size, sizeof(AcpiPackageElement));
            if (!Value->Package.Data) {
                return 0;
            }

            uint8_t i = 0;
            while (PkgLength) {
                if (i >= Value->Package.Size) {
                    free(Value->Package.Data);
                    return 0;
                }

                uint32_t Start = State->Scope->RemainingLength;
                uint8_t Opcode = *State->Scope->Code;

                /* Each PackageElement should always be either a DataRefObject (which we just call
                   ExecuteOpcode to handle), or a NameString. */
                if (Opcode != '\\' && Opcode != '^' && Opcode != 0x2E && Opcode != 0x2F &&
                    !isupper(Opcode) && Opcode != '_') {
                    Value->Package.Data[i].Type = 1;
                    if (!AcpipExecuteOpcode(State, &Value->Package.Data[i].Value)) {
                        free(Value->Package.Data);
                        return 0;
                    }
                } else {
                    if (!AcpipReadName(State)) {
                        free(Value->Package.Data);
                        return 0;
                    }
                }

                i++;
                if (Start - State->Scope->RemainingLength > PkgLength) {
                    free(Value->Package.Data);
                    return 0;
                }

                PkgLength -= Start - State->Scope->RemainingLength;
            }

            break;
        }

        /* OnesOp */
        case 0xFF: {
            Value->Type = ACPI_INTEGER;
            Value->Integer = UINT64_MAX;
            break;
        }

        /* RevisionOp */
        case 0x305B: {
            Value->Type = ACPI_INTEGER;
            Value->Integer = ACPI_REVISION;
            break;
        }

        default:
            return -1;
    }

    return 1;
}
