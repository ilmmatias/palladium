/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRY_EXECUTE_OPCODE(Function, ...)                \
    Status = Function(State, FullOpcode, ##__VA_ARGS__); \
    if (!Status) {                                       \
        return 0;                                        \
    } else if (Status > 0) {                             \
        break;                                           \
    }

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function executes the whichever AML opcode the current scope points to, updating the
 *     internal state accordingly.
 *
 * PARAMETERS:
 *     State - AML state containing the current scope.
 *     Result - Output; Where to store the resulting value; Only used for expression calls.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int AcpipExecuteOpcode(AcpipState *State, AcpiValue *Result) {
    uint8_t Opcode;
    if (!AcpipReadByte(State, &Opcode)) {
        return 0;
    }

    uint8_t ExtOpcode = 0;
    if (Opcode == 0x5B && !AcpipReadByte(State, &ExtOpcode)) {
        return 0;
    }

    uint32_t Start = State->Scope->RemainingLength;
    const uint8_t *StartCode = State->Scope->Code;

    uint16_t FullOpcode = Opcode | ((uint16_t)ExtOpcode << 8);
    AcpiValue Value;
    memset(&Value, 0, sizeof(AcpiValue));

    do {
        int Status;
        TRY_EXECUTE_OPCODE(AcpipExecuteDataObjOpcode, &Value);
        TRY_EXECUTE_OPCODE(AcpipExecuteFieldOpcode);
        TRY_EXECUTE_OPCODE(AcpipExecuteMathOpcode, &Value);
        TRY_EXECUTE_OPCODE(AcpipExecuteNamedObjOpcode);
        TRY_EXECUTE_OPCODE(AcpipExecuteNsModOpcode);

        switch (FullOpcode) {
            /* LocalObj (Local0-6) */
            case 0x60:
            case 0x61:
            case 0x62:
            case 0x63:
            case 0x64:
            case 0x65:
            case 0x66: {
                memcpy(&Value, &State->Locals[Opcode - 0x60], sizeof(AcpiValue));
                break;
            }

            /* ArgObj (Arg0-6) */
            case 0x68:
            case 0x69:
            case 0x6A:
            case 0x6B:
            case 0x6C:
            case 0x6D:
            case 0x6E: {
                memcpy(&Value, &State->Arguments[Opcode - 0x68], sizeof(AcpiValue));
                break;
            }

            /* DefStore := StoreOp TermArg SuperName */
            case 0x70: {
                if (!AcpipExecuteOpcode(State, &Value)) {
                    return 0;
                }

                AcpipTarget *Target = AcpipExecuteSuperName(State);
                if (!Target) {
                    AcpiFreeValueData(&Value);
                    return 0;
                }

                AcpipStoreTarget(State, Target, &Value);
                free(Target);

                break;
            }

            /* DefConcat := ConcatOp Data Data Target */
            case 0x73: {
                AcpiValue Left;
                if (!AcpipExecuteOpcode(State, &Left)) {
                    return 0;
                }

                AcpiValue Right;
                if (!AcpipExecuteOpcode(State, &Right)) {
                    AcpiFreeValueData(&Left);
                    return 0;
                }

                AcpipTarget *Target = AcpipExecuteTarget(State);
                if (!Target) {
                    AcpiFreeValueData(&Left);
                    AcpiFreeValueData(&Right);
                    return 0;
                }

                switch (Left.Type) {
                    /* Read it as two integers, and append them into a buffer. */
                    case ACPI_INTEGER: {
                        uint64_t LeftValue = Left.Integer;
                        uint64_t RightValue;
                        int Result = AcpipCastToInteger(&Right, &RightValue);

                        AcpiFreeValueData(&Left);
                        AcpiFreeValueData(&Right);
                        if (!Result) {
                            return 0;
                        }

                        Value.Type = ACPI_BUFFER;
                        Value.Buffer.Size = 16;
                        Value.Buffer.Data = malloc(16);
                        if (!Value.Buffer.Data) {
                            return 0;
                        }

                        *((uint64_t *)Value.Buffer.Data) = LeftValue;
                        *((uint64_t *)(Value.Buffer.Data + 8)) = RightValue;

                        break;
                    }

                    /* Read it as two buffers, and append them into another buffer. */
                    case ACPI_BUFFER: {
                        if (!AcpipCastToBuffer(&Right)) {
                            AcpiFreeValueData(&Left);
                            AcpiFreeValueData(&Right);
                            return 0;
                        }

                        Value.Type = ACPI_BUFFER;
                        Value.Buffer.Size = Left.Buffer.Size + Right.Buffer.Size;
                        Value.Buffer.Data = malloc(Value.Buffer.Size);
                        if (!Value.Buffer.Data) {
                            AcpiFreeValueData(&Left);
                            AcpiFreeValueData(&Right);
                            return 0;
                        }

                        memcpy(Value.Buffer.Data, Left.Buffer.Data, Left.Buffer.Size);
                        memcpy(
                            Value.Buffer.Data + Left.Buffer.Size,
                            Right.Buffer.Data,
                            Right.Buffer.Size);
                        AcpiFreeValueData(&Left);
                        AcpiFreeValueData(&Right);

                        break;
                    }

                    /* Convert both sides into strings, and append them into a single string. */
                    default: {
                        if (!AcpipCastToString(&Left, 1) || !AcpipCastToString(&Right, 1)) {
                            AcpiFreeValueData(&Left);
                            AcpiFreeValueData(&Right);
                            return 0;
                        }

                        Value.Type = ACPI_STRING;
                        Value.String = malloc(strlen(Left.String) + strlen(Right.String) + 1);
                        if (!Value.String) {
                            AcpiFreeValueData(&Left);
                            AcpiFreeValueData(&Right);
                            return 0;
                        }

                        strcpy(Value.String, Left.String);
                        strcat(Value.String, Right.String);
                        AcpiFreeValueData(&Left);
                        AcpiFreeValueData(&Right);

                        break;
                    }
                }

                AcpipStoreTarget(State, Target, &Value);
                free(Target);

                break;
            }

            /* DefSizeOf := SizeOfOp SuperName */
            case 0x87: {
                AcpipTarget *SuperName = AcpipExecuteSuperName(State);
                if (!SuperName) {
                    return 0;
                }

                AcpiValue *Target = AcpipReadTarget(State, SuperName);
                if (!Target) {
                    free(SuperName);
                    return 0;
                }

                free(SuperName);
                Value.Type = ACPI_INTEGER;

                if (Target->Type == ACPI_STRING) {
                    Value.Integer = strlen(Target->String);
                } else if (Target->Type == ACPI_BUFFER) {
                    Value.Integer = Target->Buffer.Size;
                } else if (Target->Type == ACPI_PACKAGE) {
                    Value.Integer = Target->Package.Size;
                } else {
                    return 0;
                }

                break;
            }

            /* DefToBuffer := ToBufferOp Operand Target */
            case 0x96: {
                if (!AcpipExecuteOpcode(State, &Value)) {
                    return 0;
                }

                AcpipTarget *Target = AcpipExecuteTarget(State);
                if (!Target) {
                    AcpiFreeValueData(&Value);
                    return 0;
                }

                if (!AcpipCastToBuffer(&Value)) {
                    free(Target);
                    AcpiFreeValueData(&Value);
                    return 0;
                }

                AcpipStoreTarget(State, Target, &Value);
                free(Target);

                break;
            }

            /* DefToHexString := ToHexStringOp Operand Target */
            case 0x98: {
                if (!AcpipExecuteOpcode(State, &Value)) {
                    return 0;
                }

                AcpipTarget *Target = AcpipExecuteTarget(State);
                if (!Target) {
                    AcpiFreeValueData(&Value);
                    return 0;
                }

                if (!AcpipCastToString(&Value, 0)) {
                    free(Target);
                    AcpiFreeValueData(&Value);
                    return 0;
                }

                AcpipStoreTarget(State, Target, &Value);
                free(Target);

                break;
            }

            /* DefIfElse := IfOp PkgLength Predicate TermList DefElse */
            case 0xA0: {
                uint32_t Length;
                if (!AcpipReadPkgLength(State, &Length)) {
                    return 0;
                }

                uint64_t Predicate;
                if (!AcpipExecuteInteger(State, &Predicate)) {
                    return 0;
                }

                uint32_t LengthSoFar = Start - State->Scope->RemainingLength;
                if (LengthSoFar > Length || Length - LengthSoFar > State->Scope->RemainingLength) {
                    return 0;
                }

                if (Predicate) {
                    AcpipScope *Scope = AcpipEnterIf(State, Length - LengthSoFar);
                    if (!Scope) {
                        return 0;
                    }

                    State->Scope = Scope;
                    break;
                }

                State->Scope->Code += Length - LengthSoFar;
                State->Scope->RemainingLength -= Length - LengthSoFar;

                /* DefElse only really matter for If(false), and we ignore it anywhere else;
                   Here, we try reading up the code (and entering the else scope) after
                   If(false). */
                if (!State->Scope->RemainingLength || *State->Scope->Code != 0xA1) {
                    break;
                }

                State->Scope->Code++;
                State->Scope->RemainingLength--;
                Start = State->Scope->RemainingLength;

                if (!AcpipReadPkgLength(State, &Length)) {
                    return 0;
                }

                LengthSoFar = Start - State->Scope->RemainingLength;
                if (LengthSoFar > Length || Length - LengthSoFar > State->Scope->RemainingLength) {
                    return 0;
                }

                AcpipScope *Scope = AcpipEnterIf(State, Length - LengthSoFar);
                if (!Scope) {
                    return 0;
                }

                State->Scope = Scope;
                break;
            }

            /* DefElse := ElseOp PkgLength TermList */
            case 0xA1: {
                uint32_t Length;
                if (!AcpipReadPkgLength(State, &Length) || Length > Start) {
                    return 0;
                }

                State->Scope->Code = StartCode + Length;
                State->Scope->RemainingLength = Start - Length;
                break;
            }

            /* DefWhile := WhileOp PkgLength Predicate TermList */
            case 0xA2: {
                uint32_t Length;
                if (!AcpipReadPkgLength(State, &Length)) {
                    return 0;
                }

                const uint8_t *PredicateStart = State->Scope->Code;
                uint32_t PredicateBacktrack = State->Scope->RemainingLength;

                uint64_t Predicate;
                if (!AcpipExecuteInteger(State, &Predicate)) {
                    return 0;
                }

                uint32_t LengthSoFar = Start - State->Scope->RemainingLength;
                if (LengthSoFar > Length || Length - LengthSoFar > State->Scope->RemainingLength) {
                    return 0;
                }

                if (!Predicate) {
                    State->Scope->Code += Length - LengthSoFar;
                    State->Scope->RemainingLength -= Length - LengthSoFar;
                    break;
                }

                AcpipScope *Scope = AcpipEnterWhile(
                    State, PredicateStart, PredicateBacktrack, Length - LengthSoFar);
                if (!Scope) {
                    return 0;
                }

                break;
            }

            default: {
                State->Scope->Code--;
                State->Scope->RemainingLength++;

                /* MethodInvocation := NameString TermArgList
                   NameString should start with either a prefix (\, ^, 0x2E or 0x2F), or a lead
                   name char, so we can quickly check if this is it. */
                if (Opcode != '\\' && Opcode != '^' && Opcode != 0x2E && Opcode != 0x2F &&
                    !isupper(Opcode) && Opcode != '_') {
                    printf(
                        "unimplemented opcode: %#hx; %d bytes left to parse out of %d.\n",
                        Opcode | ((uint32_t)ExtOpcode << 8),
                        State->Scope->RemainingLength,
                        State->Scope->Length);
                    while (1)
                        ;
                }

                AcpipName *Name = AcpipReadName(State);
                if (!Name) {
                    return 0;
                }

                AcpiObject *Object = AcpipResolveObject(Name);
                if (!Object) {
                    free(Name);
                    return 0;
                }

                /* MethodInvocation is valid on non-method items, where we just return their
                   value. */
                if (Object->Value.Type != ACPI_METHOD) {
                    if (Object->Value.Type <= ACPI_FIELD_UNIT ||
                        Object->Value.Type == ACPI_REFERENCE) {
                        memcpy(&Value, &Object->Value, sizeof(AcpiValue));
                    } else {
                        Value.Type = ACPI_REFERENCE;
                        Value.Reference = Object;
                    }

                    break;
                }

                AcpiValue Arguments[7];
                memset(Arguments, 0, sizeof(Arguments));

                /* The amount of arguments can be determined by using the method itself, which is a
                   bit annoying, as we can't parse the MethodInvocation or anything after it
                   (inside its enclosing scope) without knowing the method itself first. */
                for (int i = 0; i < (Object->Value.Method.Flags & 0x07); i++) {
                    if (!AcpipExecuteOpcode(State, &Arguments[i])) {
                        return 0;
                    }
                }

                AcpiValue *ReturnValue = AcpiExecuteMethodFromObject(
                    Object, Object->Value.Method.Flags & 0x07, Arguments);
                if (!ReturnValue) {
                    return 0;
                }

                memcpy(&Value, ReturnValue, sizeof(AcpiValue));
                free(ReturnValue);

                break;
            }
        }
    } while (0);

    if (Result) {
        memcpy(Result, &Value, sizeof(AcpiValue));
    }

    return 1;
}
