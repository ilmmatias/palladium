/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern AcpipArgument AcpipGroup0Arguments[256];
extern AcpipArgument AcpipGroup1Arguments[256];

#define TRY_EXECUTE_OPCODE(Function, ...)                    \
    Status = Function(State, Opcode->Opcode, ##__VA_ARGS__); \
    if (!Status) {                                           \
        return 0;                                            \
    } else if (Status > 0) {                                 \
        break;                                               \
    }

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function prepares for executing the next opcode. You should call this before
 *     ExecuteOpcode.
 *
 * PARAMETERS:
 *     State - AML state containing the current scope.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int AcpipPrepareExecuteOpcode(AcpipState *State) {
    uint8_t Opcode;
    if (!AcpipReadByte(State, &Opcode)) {
        return 0;
    }

    uint8_t ExtOpcode = 0;
    if (Opcode == 0x5B && !AcpipReadByte(State, &ExtOpcode)) {
        return 0;
    }

    AcpipArgument *Arguments =
        Opcode == 0x5B ? &AcpipGroup1Arguments[ExtOpcode] : &AcpipGroup0Arguments[Opcode];
    if (!Arguments->Valid) {
        return 0;
    }

    AcpipOpcode *Info = calloc(1, sizeof(AcpipOpcode));
    if (!Info) {
        return 0;
    }

    Info->StartCode = State->Scope->Code;
    Info->Start = State->Scope->RemainingLength;
    Info->Opcode = ((uint16_t)ExtOpcode << 8) | Opcode;
    Info->ArgInfo = Arguments;
    Info->Parent = State->Opcode;
    State->Opcode = Info;
    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements the main argument parsing+op execution loop; This should be called
 *     after PrepareExecuteOpcode.
 *
 * PARAMETERS:
 *     State - AML state containing the current scope.
 *     Result - Output; Where to store the resulting value; Only used for expression calls.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int AcpipExecuteOpcode(AcpipState *State, AcpiValue *Result) {
    while (1) {
        AcpipOpcode *Opcode = State->Opcode;

        if (Opcode->ArgInfo->HasPkgLength && !Opcode->ValidPkgLength) {
            if (!AcpipReadPkgLength(State, &Opcode->PkgLength)) {
                return 0;
            }

            Opcode->ValidPkgLength = 1;
            Opcode->Predicate = State->Scope->Code;
            Opcode->PredicateBacktrack = State->Scope->RemainingLength;
        }

        /* We're parsing one argument at a time, as anything TermArg-like might need to enter the
           ExecuteOpcode loop too (before keeping on parsing this op). */
        if (Opcode->ValidArgs < Opcode->ArgInfo->Count) {
            int Position = Opcode->ValidArgs++;
            AcpipArgumentValue *Arg = &Opcode->FixedArguments[Position];

            switch (Opcode->ArgInfo->Types[Position]) {
                /* This shouldn't really be reached? */
                case ACPI_ARG_NONE:
                    AcpipShowDebugMessage("trying to read EMPTY argument, possible memory "
                                          "corruption (or code bug)?\n");
                    break;

                case ACPI_ARG_BYTE:
                    if (!AcpipReadByte(State, (uint8_t *)&Arg->Integer)) {
                        return 0;
                    }

                    break;

                case ACPI_ARG_WORD:
                    if (!AcpipReadWord(State, (uint16_t *)&Arg->Integer)) {
                        return 0;
                    }

                    break;

                case ACPI_ARG_DWORD:
                    if (!AcpipReadDWord(State, (uint32_t *)&Arg->Integer)) {
                        return 0;
                    }

                    break;

                case ACPI_ARG_QWORD:
                    if (!AcpipReadQWord(State, &Arg->Integer)) {
                        return 0;
                    }

                    break;

                case ACPI_ARG_STRING: {
                    size_t StringSize = 0;
                    while (StringSize < State->Scope->RemainingLength &&
                           State->Scope->Code[StringSize]) {
                        StringSize++;
                    }

                    if (++StringSize > State->Scope->RemainingLength) {
                        return 0;
                    }

                    Arg->String = malloc(sizeof(AcpiString) + StringSize);
                    if (!Arg->String) {
                        return 0;
                    }

                    Arg->String->References = 1;
                    memcpy(Arg->String->Data, State->Scope->Code, StringSize);
                    State->Scope->Code += StringSize;
                    State->Scope->RemainingLength -= StringSize;

                    break;
                }

                case ACPI_ARG_NAME:
                    if (!AcpipReadName(State, &Arg->Name)) {
                        return 0;
                    }

                    break;

                default:
                    if (!AcpipPrepareExecuteOpcode(State)) {
                        return 0;
                    }

                    State->Opcode->ParentArgType = Opcode->ArgInfo->Types[Position];
                    State->Opcode->ParentArg = Arg;
                    break;
            }

            continue;
        }

        /* We still can't break, because of the variable argument list that some ops have. */

        AcpiValue Value;
        memset(&Value, 0, sizeof(AcpiValue));

        do {
            int Status;
            int ObjReference = Opcode->ParentArg && Opcode->ParentArgType == ACPI_ARG_OBJ_REF;
            int WeakReference = ObjReference && Opcode->Parent->Opcode == 0x125B;

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

            switch (Opcode->Opcode) {
                /* LocalObj (Local0-7) */
                case 0x60:
                case 0x61:
                case 0x62:
                case 0x63:
                case 0x64:
                case 0x65:
                case 0x66:
                case 0x67: {
                    if (ObjReference) {
                        Value.Type = ACPI_LOCAL;
                        Value.Integer = Opcode->Opcode - 0x60;
                    } else {
                        memcpy(&Value, &State->Locals[Opcode->Opcode - 0x60], sizeof(AcpiValue));
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
                        Value.Integer = Opcode->Opcode - 0x68;
                    } else {
                        memcpy(&Value, &State->Arguments[Opcode->Opcode - 0x68], sizeof(AcpiValue));
                    }

                    break;
                }

                /* DefStore := StoreOp TermArg SuperName */
                case 0x70: {
                    AcpiValue *Target = &State->Opcode->FixedArguments[1].TermArg;

                    if (!AcpipStoreTarget(
                            State, Target, &State->Opcode->FixedArguments[0].TermArg)) {
                        AcpiRemoveReference(Target, 0);
                        return 0;
                    }

                    AcpiRemoveReference(Target, 0);
                    break;
                }

                /* DefSizeOf := SizeOfOp SuperName */
                case 0x87: {
                    AcpiValue Target;
                    if (!AcpipReadTarget(
                            State, &State->Opcode->FixedArguments[0].TermArg, &Target)) {
                        AcpiRemoveReference(&State->Opcode->FixedArguments[0].TermArg, 0);
                        return 0;
                    }

                    Value.Type = ACPI_INTEGER;
                    Value.References = 1;

                    AcpiRemoveReference(&State->Opcode->FixedArguments[0].TermArg, 0);
                    if (Target.Type == ACPI_STRING) {
                        Value.Integer = strlen(Target.String->Data);
                    } else if (Target.Type == ACPI_BUFFER) {
                        Value.Integer = Target.Buffer->Size;
                    } else if (Target.Type == ACPI_PACKAGE) {
                        Value.Integer = Target.Package->Size;
                    } else {
                        return 0;
                    }

                    break;
                }

                /* DefCopyObject := CopyObjectOp TermArg SimpleName */
                case 0x9D: {
                    AcpiValue *Source = &State->Opcode->FixedArguments[0].TermArg;
                    AcpiValue *Target = &State->Opcode->FixedArguments[1].TermArg;

                    AcpiValue Copy;
                    if (!AcpiCopyValue(Source, &Copy)) {
                        AcpiRemoveReference(Source, 0);
                        AcpiRemoveReference(Target, 0);
                        return 0;
                    }

                    if (!AcpipStoreTarget(State, Target, &Copy)) {
                        AcpiRemoveReference(Source, 0);
                        AcpiRemoveReference(Target, 0);
                        return 0;
                    }

                    AcpiRemoveReference(Source, 0);
                    AcpiRemoveReference(Target, 0);
                    break;
                }

                /* DefSleep := SleepOp MsecTime */
                case 0x225B: {
                    /* TODO: Just a stub at the moment, make it work after we implement support in
                       the kernel. */
                    break;
                }

                /* DebugObj */
                case 0x315B: {
                    Value.Type = ACPI_DEBUG;
                    break;
                }

                default: {
                    State->Scope->Code--;
                    State->Scope->RemainingLength++;

                    /* MethodInvocation := NameString TermArgList
                       NameString should start with either a prefix (\, ^, 0x2E or 0x2F), or a lead
                       name char, so we can quickly check if this is it. */
                    if (Opcode->Opcode != '\\' && Opcode->Opcode != '^' && Opcode->Opcode != 0x2E &&
                        Opcode->Opcode != 0x2F && !isupper(Opcode->Opcode) &&
                        Opcode->Opcode != '_') {
                        AcpipShowErrorMessage(
                            ACPI_REASON_CORRUPTED_TABLES,
                            "unimplemented opcode %04hx\n",
                            Opcode->Opcode);
                    }

                    AcpiName Name;
                    if (!AcpipReadName(State, &Name)) {
                        return 0;
                    }

                    AcpiObject *Object = AcpipResolveObject(&Name);
                    if (!Object) {
                        if (WeakReference) {
                            Value.Type = ACPI_EMPTY;
                            break;
                        } else {
                            return 0;
                        }
                    }

                    if (ObjReference) {
                        Value.Type = ACPI_REFERENCE;
                        Value.Reference = Object;
                        break;
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

                    /* The amount of arguments can be determined by using the method itself, which
                       is a bit annoying, as we can't parse the MethodInvocation or anything after
                       it (inside its enclosing scope) without knowing the method itself first. */
                    for (int i = 0; i < (Object->Value.Method.Flags & 0x07); i++) {
                        if (!AcpipPrepareExecuteOpcode(State) ||
                            !AcpipExecuteOpcode(State, &Arguments[i])) {
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

        /* If we're done, we should be free to write the result variable (as we're returning
           already). */
        if (!Opcode->ParentArg) {
            if (Result) {
                memcpy(Result, &Value, sizeof(AcpiValue));
            } else {
                AcpiRemoveReference(&Value, 0);
            }

            AcpipOpcode *Parent = Opcode->Parent;
            free(Opcode);
            State->Opcode = Parent;

            return 1;
        }

        /* Everything but the primitives (integer, buffer, and package) need no handling;
           Otherwise, we need to CastToX() before writing into the TargetArg. */
        switch (Opcode->ParentArgType) {
            case ACPI_ARG_INTEGER:
                Opcode->ParentArg->TermArg.Type = ACPI_INTEGER;
                if (!AcpipCastToInteger(&Value, &Opcode->ParentArg->TermArg.Integer, 1)) {
                    return 0;
                }

                break;

            case ACPI_ARG_BUFFER:
                memcpy(&Opcode->ParentArg->TermArg, &Value, sizeof(AcpiValue));
                if (!AcpipCastToBuffer(&Opcode->ParentArg->TermArg)) {
                    return 0;
                }

                break;

            case ACPI_ARG_PACKAGE:
                AcpipShowErrorMessage(
                    ACPI_REASON_CORRUPTED_TABLES,
                    "we need to write the result into %p, the type is ACPI_ARG_PACKAGE\n",
                    Opcode->ParentArg);

            default:
                memcpy(&Opcode->ParentArg->TermArg, &Value, sizeof(AcpiValue));
                break;
        }

        AcpipOpcode *Parent = Opcode->Parent;
        free(Opcode);
        State->Opcode = Parent;
    }
}
