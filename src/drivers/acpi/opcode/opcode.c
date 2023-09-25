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
 *     ObjReference - Set this to 1 if you're parsing an object reference (changes the inner
 *                    workings of a few opcodes).
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int AcpipExecuteOpcode(AcpipState *State, AcpiValue *Result, int ObjReference) {
    uint8_t Opcode;
    if (!AcpipReadByte(State, &Opcode)) {
        return 0;
    }

    uint8_t ExtOpcode = 0;
    if (Opcode == 0x5B && !AcpipReadByte(State, &ExtOpcode)) {
        return 0;
    }

    uint16_t FullOpcode = Opcode | ((uint16_t)ExtOpcode << 8);
    AcpiValue Value;
    memset(&Value, 0, sizeof(AcpiValue));

    do {
        int Status;

        TRY_EXECUTE_OPCODE(AcpipExecuteConcatOpcode, &Value);
        TRY_EXECUTE_OPCODE(AcpipExecuteConvOpcode, &Value);
        TRY_EXECUTE_OPCODE(AcpipExecuteDataObjOpcode, &Value);
        TRY_EXECUTE_OPCODE(AcpipExecuteFieldOpcode);
        TRY_EXECUTE_OPCODE(AcpipExecuteLockOpcode, &Value);
        TRY_EXECUTE_OPCODE(AcpipExecuteMathOpcode, &Value);
        TRY_EXECUTE_OPCODE(AcpipExecuteNamedObjOpcode);
        TRY_EXECUTE_OPCODE(AcpipExecuteNsModOpcode);
        TRY_EXECUTE_OPCODE(AcpipExecuteRefOpcode, &Value);
        TRY_EXECUTE_OPCODE(AcpipExecuteStmtOpcode);

        switch (FullOpcode) {
            /* LocalObj (Local0-6) */
            case 0x60:
            case 0x61:
            case 0x62:
            case 0x63:
            case 0x64:
            case 0x65:
            case 0x66: {
                if (ObjReference) {
                    Value.Type = ACPI_LOCAL;
                    Value.Integer = FullOpcode - 0x60;
                } else {
                    memcpy(&Value, &State->Locals[Opcode - 0x60], sizeof(AcpiValue));
                }

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
                if (ObjReference) {
                    Value.Type = ACPI_ARG;
                    Value.Integer = FullOpcode - 0x68;
                } else {
                    memcpy(&Value, &State->Arguments[Opcode - 0x68], sizeof(AcpiValue));
                }

                break;
            }

            /* DefStore := StoreOp TermArg SuperName */
            case 0x70: {
                AcpiValue Source;
                if (!AcpipExecuteOpcode(State, &Source, 0)) {
                    return 0;
                }

                AcpipTarget Target;
                if (!AcpipExecuteSuperName(State, &Target, 0)) {
                    AcpiRemoveReference(&Source, 0);
                    return 0;
                }

                if (!AcpipStoreTarget(State, &Target, &Source)) {
                    return 0;
                }

                break;
            }

            /* DefSizeOf := SizeOfOp SuperName */
            case 0x87: {
                AcpipTarget SuperName;
                if (!AcpipExecuteSuperName(State, &SuperName, 0)) {
                    return 0;
                }

                AcpiValue Target;
                if (!AcpipReadTarget(State, &SuperName, &Target)) {
                    return 0;
                }

                Value.Type = ACPI_INTEGER;
                Value.References = 1;

                if (Target.Type == ACPI_STRING) {
                    Value.Integer = strlen(Target.String->Data);
                } else if (Target.Type == ACPI_BUFFER) {
                    Value.Integer = Target.Buffer->Size;
                } else if (Target.Type == ACPI_PACKAGE) {
                    Value.Integer = Target.Package->Size;
                } else {
                    return 0;
                }

                AcpiRemoveReference(&Target, 0);

                break;
            }

            /* DefCopyObject := CopyObjectOp TermArg SimpleName */
            case 0x9D: {
                AcpiValue Source;
                if (!AcpipExecuteOpcode(State, &Source, 0)) {
                    return 0;
                }

                AcpipTarget Target;
                if (!AcpipExecuteSimpleName(State, &Target)) {
                    AcpiRemoveReference(&Source, 0);
                    return 0;
                }

                AcpiValue Copy;
                if (!AcpiCopyValue(&Source, &Copy)) {
                    AcpiRemoveReference(&Source, 0);
                    return 0;
                }

                AcpiRemoveReference(&Source, 0);
                if (!AcpipStoreTarget(State, &Target, &Copy)) {
                    return 0;
                }

                break;
            }

            /* DefSleep := SleepOp MsecTime */
            case 0x225B: {
                /* TODO: Just a stub at the moment, make it work after we implement support in
                   the kernel. */

                uint64_t Milliseconds;
                if (!AcpipExecuteInteger(State, &Milliseconds)) {
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
                    AcpipShowErrorMessage(
                        ACPI_REASON_CORRUPTED_TABLES,
                        "unimplemented opcode: %#hx; %d bytes left to parse out of %d.\n",
                        Opcode | ((uint32_t)ExtOpcode << 8),
                        State->Scope->RemainingLength,
                        State->Scope->Length);
                }

                AcpipName Name;
                if (!AcpipReadName(State, &Name)) {
                    return 0;
                }

                AcpiObject *Object = AcpipResolveObject(&Name);
                if (!Object) {
                    return 0;
                }

                /* MethodInvocation is valid on non-method items, where we just return their
                   value. */
                if (Object->Value.Type != ACPI_METHOD) {
                    switch (Object->Value.Type) {
                        /* Field units will get redirected to the right read type (MMIO, PCI,
                           etc). */
                        case ACPI_FIELD_UNIT:
                            if (!AcpipReadField(&Object->Value, &Value)) {
                                return 0;
                            }

                            break;

                        /* Anything else we just mount a reference to. */
                        default:
                            AcpiCreateReference(&Object->Value, &Value);
                            break;
                    }

                    break;
                }

                AcpiValue Arguments[7];
                memset(Arguments, 0, sizeof(Arguments));

                /* The amount of arguments can be determined by using the method itself, which is a
                   bit annoying, as we can't parse the MethodInvocation or anything after it
                   (inside its enclosing scope) without knowing the method itself first. */
                for (int i = 0; i < (Object->Value.Method.Flags & 0x07); i++) {
                    if (!AcpipExecuteOpcode(State, &Arguments[i], 0)) {
                        return 0;
                    }
                }

                if (!AcpiExecuteMethod(
                        Object, Object->Value.Method.Flags & 0x07, Arguments, &Value)) {
                    return 0;
                }

                for (int i = 0; i < (Object->Value.Method.Flags & 0x07); i++) {
                    AcpiRemoveReference(&Arguments[i], 0);
                }

                break;
            }
        }
    } while (0);

    if (Result) {
        memcpy(Result, &Value, sizeof(AcpiValue));
    }

    return 1;
}
