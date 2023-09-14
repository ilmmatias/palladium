/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries reading the field list attached to the current field or bank/index
 *     field.
 *
 * PARAMETERS:
 *     State - AML state containing the current scope.
 *     Base - Value to be used as base for each field unit.
 *     Start - RemainingLength after reading just the opcode.
 *     Length - Length of this package.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
static int ReadFieldList(AcpipState *State, AcpiValue *Base, uint32_t Start, uint32_t Length) {
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
                Value.FieldUnit.AccessType = AccessType;
                Value.FieldUnit.AccessAttrib = AccessAttrib;
                Value.FieldUnit.AccessLength = AccessLength;
                Value.FieldUnit.Offset = Offset;
                Value.FieldUnit.Length = Length;
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

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries the execute the given opcode as a Field opcode.
 *     This contains all field related ops from the `20.2.5.2. Named Objects Encoding` section in
 *     the AML spec.
 *
 * PARAMETERS:
 *     State - AML state containing the current scope.
 *     Opcode - Opcode to be executed; The high bits contain the extended opcode number (when the
 *              lower opcode is 0x5B).
 *
 * RETURN VALUE:
 *     Positive number on success, negative on "this isn't a field op", 0 on failure.
 *-----------------------------------------------------------------------------------------------*/
int AcpipExecuteFieldOpcode(AcpipState *State, uint16_t Opcode) {
    uint32_t Start = State->Scope->RemainingLength;

    switch (Opcode) {
        /* DefCreateDWordField := CreateDWordFieldOp SourceBuff ByteIndex NameString
           DefCreateWordField := CreateWordFieldOp SourceBuff ByteIndex NameString
           DefCreateByteField := CreateByteFieldOp SourceBuff ByteIndex NameString
           DefCreateQWordField := CreateQWordFieldOp SourceBuff ByteIndex NameString */
        case 0x8A:
        case 0x8B:
        case 0x8C:
        case 0x8F: {
            AcpiValue SourceBuff;
            if (!AcpipExecuteBuffer(State, &SourceBuff)) {
                return 0;
            }

            uint64_t ByteIndex;
            if (!AcpipExecuteInteger(State, &ByteIndex)) {
                AcpiRemoveReference(&SourceBuff, 0);
                return 0;
            }

            AcpipName *Name = AcpipReadName(State);
            if (!Name) {
                AcpiRemoveReference(&SourceBuff, 0);
                return 0;
            }

            /* .BufferField.FieldSource is a pointer into the heap, so let's allocate somewhere to
               store SourceBuff. */
            AcpiValue *Buffer = malloc(sizeof(AcpiValue));
            if (!Buffer) {
                free(Name);
                AcpiRemoveReference(&SourceBuff, 0);
                return 0;
            }

            memcpy(Buffer, &SourceBuff, sizeof(AcpiValue));

            AcpiValue Value;
            Value.Type = ACPI_BUFFER_FIELD;
            Value.References = 1;
            Value.BufferField.FieldSource = Buffer;
            Value.BufferField.Index = ByteIndex * 8;
            Value.BufferField.Size = Opcode == 0x8A   ? 32
                                     : Opcode == 0x8B ? 16
                                     : Opcode == 0x8C ? 8
                                                      : 64;

            if (!AcpipCreateObject(Name, &Value)) {
                free(Name);
                AcpiRemoveReference(&SourceBuff, 0);
                return 0;
            }

            break;
        }

        /* DefField := FieldOp PkgLength NameString FieldFlags FieldList */
        case 0x815B: {
            uint32_t Length;
            if (!AcpipReadPkgLength(State, &Length)) {
                return 0;
            }

            AcpipName *Name = AcpipReadName(State);
            if (!Name) {
                return 0;
            }

            AcpiObject *Object = AcpipResolveObject(Name);
            if (!Object) {
                free(Name);
                return 0;
            } else if (Object->Value.Type != ACPI_REGION) {
                return 0;
            }

            AcpiValue Base;
            Base.Type = ACPI_FIELD_UNIT;
            Base.References = 1;
            Base.FieldUnit.FieldType = ACPI_FIELD;
            Base.FieldUnit.Region = Object;
            Base.FieldUnit.Data = NULL;
            if (!ReadFieldList(State, &Base, Start, Length)) {
                return 0;
            }

            break;
        }

        /* DefIndexField := IndexFieldOp PkgLength NameString NameString FieldFlags FieldList */
        case 0x865B: {
            uint32_t Length;
            if (!AcpipReadPkgLength(State, &Length)) {
                return 0;
            }

            AcpipName *IndexName = AcpipReadName(State);
            if (!IndexName) {
                return 0;
            }

            AcpiObject *IndexObject = AcpipResolveObject(IndexName);
            if (!IndexObject) {
                free(IndexName);
                return 0;
            }

            AcpipName *DataName = AcpipReadName(State);
            if (!DataName) {
                return 0;
            }

            AcpiObject *DataObject = AcpipResolveObject(DataName);
            if (!DataObject) {
                free(DataName);
                return 0;
            }

            AcpiValue Base;
            Base.Type = ACPI_FIELD_UNIT;
            Base.References = 1;
            Base.FieldUnit.FieldType = ACPI_INDEX_FIELD;
            Base.FieldUnit.Region = IndexObject;
            Base.FieldUnit.Data = DataObject;
            if (!ReadFieldList(State, &Base, Start, Length)) {
                return 0;
            }

            break;
        }

        default:
            return -1;
    }

    return 1;
}
