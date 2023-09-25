/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRY_EXECUTE_OPCODE(Function, ...)                        \
    Status = Function(State, FullOpcode, Target, ##__VA_ARGS__); \
    if (!Status) {                                               \
        return 0;                                                \
    } else if (Status > 0) {                                     \
        return 1;                                                \
    }

static int
ExecuteSimpleName(AcpipState *State, uint16_t Opcode, AcpipTarget *Target, int Optional) {
    switch (Opcode) {
        /* LocalObj (Local0-6) */
        case 0x60:
        case 0x61:
        case 0x62:
        case 0x63:
        case 0x64:
        case 0x65:
        case 0x66:
            Target->Type = ACPI_TARGET_LOCAL;
            Target->Index = Opcode - 0x60;
            break;

        /* ArgObj (Arg0-6) */
        case 0x68:
        case 0x69:
        case 0x6A:
        case 0x6B:
        case 0x6C:
        case 0x6D:
        case 0x6E:
            Target->Type = ACPI_TARGET_ARG;
            Target->Index = Opcode - 0x68;
            break;

        default:
            /* SimpleName is allowed to be a NameString (store to named field/object). */
            if (Opcode != '\\' && Opcode != '^' && Opcode != 0x2E && Opcode != 0x2F &&
                !isupper(Opcode) && Opcode != '_') {
                return -1;
            }

            State->Scope->Code--;
            State->Scope->RemainingLength++;

            AcpipName Name;
            if (!AcpipReadName(State, &Name)) {
                return 0;
            }

            AcpiObject *Object = AcpipResolveObject(&Name);
            if (!Object) {
                if (Optional) {
                    Target->Type = ACPI_TARGET_UNRESOLVED;
                    return 1;
                } else {
                    return 0;
                }
            }

            Target->Type = ACPI_TARGET_NAMED;
            Target->Object = Object;
            break;
    }

    return 1;
}

static int ExecuteSuperName(AcpipState *State, uint16_t Opcode, AcpipTarget *Target) {
    switch (Opcode) {
        /* DefIndex := IndexOp BuffPkgStrObj IndexValue Target */
        case 0x88: {
            /* We probably shouldn't be repeating all this code (it already exists on ref.c). */
            AcpiValue *Buffer = malloc(sizeof(AcpiValue));
            if (!Buffer) {
                return 0;
            } else if (!AcpipExecuteOpcode(State, Buffer, 0)) {
                free(Buffer);
                return 0;
            }

            if (Buffer->Type != ACPI_STRING && Buffer->Type != ACPI_BUFFER &&
                Buffer->Type != ACPI_PACKAGE) {
                AcpiRemoveReference(Buffer, 1);
                return 0;
            }

            uint64_t Index;
            if (!AcpipExecuteInteger(State, &Index)) {
                AcpiRemoveReference(Buffer, 1);
                return 0;
            }

            AcpipTarget WriteTarget;
            if (!AcpipExecuteTarget(State, &WriteTarget)) {
                AcpiRemoveReference(Buffer, 1);
                return 0;
            }

            /* Pre-validate the index value (prevent buffer overflows). */
            switch (Buffer->Type) {
                case ACPI_STRING: {
                    if (Index > strlen(Buffer->String->Data)) {
                        AcpiRemoveReference(Buffer, 1);
                        return 0;
                    }

                    break;
                }

                case ACPI_BUFFER: {
                    if (Index >= Buffer->Buffer->Size) {
                        AcpiRemoveReference(Buffer, 1);
                        return 0;
                    }

                    break;
                }

                default: {
                    if (Index >= Buffer->Package->Size) {
                        AcpiRemoveReference(Buffer, 1);
                        return 0;
                    }

                    break;
                }
            }

            Target->Type = ACPI_TARGET_INDEX;
            Target->Source = Buffer;
            Target->Index = Index;

            AcpiValue Value;
            Value.Type = ACPI_INDEX;
            Value.References = 1;
            Value.Index.Source = Buffer;
            Value.Index.Index = Index;
            if (!AcpipStoreTarget(State, &WriteTarget, &Value)) {
                AcpiRemoveReference(Buffer, 1);
                return 0;
            }

            break;
        }

        /* DebugObj; Noop for us. */
        case 0x315B: {
            Target->Type = ACPI_TARGET_NONE;
            break;
        }

        default:
            return -1;
    }

    return 1;
}

int AcpipExecuteSimpleName(AcpipState *State, AcpipTarget *Target) {
    uint8_t Opcode;
    if (!AcpipReadByte(State, &Opcode)) {
        return 0;
    }

    uint8_t ExtOpcode = 0;
    if (Opcode == 0x5B && !AcpipReadByte(State, &ExtOpcode)) {
        return 0;
    }

    uint16_t FullOpcode = Opcode | ((uint16_t)ExtOpcode << 8);
    return ExecuteSimpleName(State, FullOpcode, Target, 0) <= 0 ? 0 : 1;
}

int AcpipExecuteSuperName(AcpipState *State, AcpipTarget *Target, int Optional) {
    uint8_t Opcode;
    if (!AcpipReadByte(State, &Opcode)) {
        return 0;
    }

    uint8_t ExtOpcode = 0;
    if (Opcode == 0x5B && !AcpipReadByte(State, &ExtOpcode)) {
        return 0;
    }

    uint16_t FullOpcode = Opcode | ((uint16_t)ExtOpcode << 8);
    do {
        int Status;
        TRY_EXECUTE_OPCODE(ExecuteSimpleName, Optional);
        TRY_EXECUTE_OPCODE(ExecuteSuperName);
    } while (0);

    printf(
        "unimplemented supername opcode: %#hx; %d bytes left to parse out of %d.\n",
        FullOpcode,
        State->Scope->RemainingLength,
        State->Scope->Length);
    while (1)
        ;
}

int AcpipExecuteTarget(AcpipState *State, AcpipTarget *Target) {
    uint8_t Opcode;
    if (!AcpipReadByte(State, &Opcode)) {
        return 0;
    }

    uint8_t ExtOpcode = 0;
    if (Opcode == 0x5B && !AcpipReadByte(State, &ExtOpcode)) {
        return 0;
    }

    uint16_t FullOpcode = Opcode | ((uint16_t)ExtOpcode << 8);
    do {
        int Status;
        TRY_EXECUTE_OPCODE(ExecuteSimpleName, 0);
        TRY_EXECUTE_OPCODE(ExecuteSuperName);

        switch (Opcode) {
            /* Assume writes to Zero are to be ignored. */
            case 0x00:
                Target->Type = ACPI_TARGET_NONE;
                break;

            default: {
                printf(
                    "unimplemented target opcode: %#hx; %d bytes left to parse out of %d.\n",
                    FullOpcode,
                    State->Scope->RemainingLength,
                    State->Scope->Length);
                while (1)
                    ;
            }
        }
    } while (0);

    return 1;
}

int AcpipReadTarget(AcpipState *State, AcpipTarget *Target, AcpiValue *Value) {
    AcpiValue *Source;

    switch (Target->Type) {
        case ACPI_TARGET_LOCAL:
            Source = &State->Locals[Target->Index];
            break;
        case ACPI_TARGET_ARG:
            Source = &State->Arguments[Target->Index];
            break;
        case ACPI_TARGET_NAMED:
            AcpiCreateReference(&Target->Object->Value, Value);
            return 1;
        default:
            return 0;
    }

    return AcpiCopyValue(Source, Value);
}

int AcpipStoreTarget(AcpipState *State, AcpipTarget *Target, AcpiValue *Value) {
    switch (Target->Type) {
        case ACPI_TARGET_LOCAL:
            AcpiRemoveReference(&State->Locals[Target->Index], 0);
            memcpy(&State->Locals[Target->Index], Value, sizeof(AcpiValue));
            break;
        case ACPI_TARGET_ARG:
            AcpiRemoveReference(&State->Arguments[Target->Index], 0);
            memcpy(&State->Arguments[Target->Index], Value, sizeof(AcpiValue));
            break;
        case ACPI_TARGET_NAMED:
            switch (Target->Object->Value.Type) {
                /* Integers, strings, and buffers are allowed to be implicitly cast into. */
                case ACPI_INTEGER: {
                    uint64_t IntegerValue;
                    if (!AcpipCastToInteger(Value, &IntegerValue, 1)) {
                        return 0;
                    }

                    AcpiRemoveReference(&Target->Object->Value, 0);
                    Target->Object->Value.Type = ACPI_INTEGER;
                    Target->Object->Value.Integer = IntegerValue;

                    break;
                }

                case ACPI_STRING: {
                    AcpiRemoveReference(&Target->Object->Value, 0);
                    memcpy(&Target->Object->Value, Value, sizeof(AcpiValue));

                    if (!AcpipCastToString(&Target->Object->Value, 1, 0)) {
                        return 0;
                    }

                    break;
                }

                case ACPI_BUFFER: {
                    AcpiRemoveReference(&Target->Object->Value, 0);
                    memcpy(&Target->Object->Value, Value, sizeof(AcpiValue));

                    if (!AcpipCastToBuffer(&Target->Object->Value)) {
                        return 0;
                    }

                    break;
                }

                /* Store to packages is only allowed if the source too is a package. */
                case ACPI_PACKAGE: {
                    if (Value->Type != ACPI_PACKAGE) {
                        return 0;
                    }

                    AcpiRemoveReference(&Target->Object->Value, 0);
                    memcpy(&Target->Object->Value, Value, sizeof(AcpiValue));

                    break;
                }

                /* Store to fields is only allowed for buffer-like sources (integers, strings, and
                   buffers). */
                case ACPI_FIELD_UNIT: {
                    if (Value->Type != ACPI_INTEGER && Value->Type != ACPI_STRING &&
                        Value->Type != ACPI_BUFFER) {
                        return 0;
                    }

                    int Status = AcpipWriteField(&Target->Object->Value, Value);
                    AcpiRemoveReference(Value, 0);

                    if (!Status) {
                        return 0;
                    }

                    break;
                }

                case ACPI_BUFFER_FIELD: {
                    uint64_t Index = Target->Object->Value.BufferField.Index;
                    AcpiValue *FieldSource = Target->Object->Value.BufferField.FieldSource;

                    uint64_t Slot;
                    if (!AcpipCastToInteger(Value, &Slot, 1)) {
                        return 0;
                    }

                    switch (Target->Object->Value.BufferField.Size) {
                        case 2:
                            *((uint16_t *)(FieldSource->Buffer->Data + Index)) = Slot;
                            break;
                        case 4:
                            *((uint32_t *)(FieldSource->Buffer->Data + Index)) = Slot;
                            break;
                        case 8:
                            *((uint64_t *)(FieldSource->Buffer->Data + Index)) = Slot;
                            break;
                        default:
                            *(FieldSource->Buffer->Data + Index) = Slot;
                            break;
                    }

                    break;
                }

                default:
                    printf("Writing to a named field of type %d\n", Target->Object->Value.Type);
                    while (1)
                        ;
            }
    }

    return 1;
}
