/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <stdlib.h>
#include <string.h>

static AcpiObject ObjectTreeRoot;
static AcpiChildren ObjectTreeRootChildren;

extern AcpiObject *AcpipObjectTree;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function starts execution of an AML method, based on a previously searched object.
 *
 * PARAMETERS:
 *     Object - Object containing the method.
 *     ArgCount - Amount of arguments to pass.
 *     Arguments - Data of the arguments.
 *     Result - Output; Where to store the return value of the method; Optional, will be ignored
 *              if NULL.
 *
 * RETURN VALUE:
 *     Return value of the method, or NULL if something went wrong.
 *-----------------------------------------------------------------------------------------------*/
int AcpiExecuteMethod(AcpiObject *Object, int ArgCount, AcpiValue *Arguments, AcpiValue *Result) {
    if (!Object || Object->Value.Type != ACPI_METHOD) {
        if (Object) {
            char *Path = AcpiGetObjectPath(Object);
            if (Path) {
                AcpipShowDebugMessage(
                    "attempt at executing non-method object, full path %s\n", Path);
                free(Path);
            } else {
                AcpipShowDebugMessage(
                    "attempt at executing non-method object, top most name %.4s\n", Object->Name);
            }
        }

        return 0;
    } else if (ArgCount < 0) {
        ArgCount = 0;
    } else if (ArgCount > 7) {
        ArgCount = 7;
    } else if (Object->Value.Method.Override) {
        return Object->Value.Method.Override(ArgCount, Arguments, Result);
    }

    AcpipState State;
    AcpipScope Scope;

    Scope.LinkedObject = Object;
    Scope.Predicate = NULL;
    Scope.PredicateBacktrack = 0;
    Scope.Code = Object->Value.Method.Start;
    Scope.Length = Object->Value.Method.Size;
    Scope.RemainingLength = Object->Value.Method.Size;
    Scope.Parent = NULL;

    memset(&State, 0, sizeof(State));
    State.IsMethod = 1;
    State.Scope = &Scope;
    State.Opcode = NULL;

    if (ArgCount) {
        memcpy(State.Arguments, Arguments, ArgCount * sizeof(AcpiValue));
    }

    int Status = AcpipExecuteTermList(&State);

    /* Objects defined inside methods have temporary scopes (they live as long as the method
       doesn't return), so we still need to cleanup (even on failure). */
    AcpiObject *Base = Object->Value.Children->Objects;
    Object->Value.Children->Objects = NULL;

    while (Base != NULL) {
        AcpiObject *Next = Base->Next;
        AcpiRemoveReference(&Base->Value, 0);
        free(Base);
        Base = Next;
    }

    if (Result && Status) {
        memcpy(Result, &State.ReturnValue, sizeof(AcpiValue));
    }

    return Status;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function does a copy of the specified object (including anything malloc'ed inside it).
 *
 * PARAMETERS:
 *     Source - Value to be copied.
 *     Target - Destination of the copy.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int AcpiCopyValue(AcpiValue *Source, AcpiValue *Target) {
    if (!Source || !Target) {
        return 0;
    }

    memcpy(Target, Source, sizeof(AcpiValue));
    Target->References = 1;

    switch (Source->Type) {
        case ACPI_STRING:
            Target->String = malloc(sizeof(AcpiString) + strlen(Source->String->Data) + 1);
            if (!Target->String) {
                return 0;
            }

            Target->String->References = 1;
            memcpy(Target->String->Data, Source->String->Data, strlen(Source->String->Data) + 1);

            break;

        case ACPI_BUFFER:
            Target->Buffer = malloc(sizeof(AcpiBuffer) + Source->Buffer->Size);
            if (!Target->Buffer) {
                return 0;
            }

            Target->Buffer->References = 1;
            Target->Buffer->Size = Source->Buffer->Size;
            memcpy(Target->Buffer->Data, Source->Buffer->Data, Source->Buffer->Size);

            break;

        case ACPI_PACKAGE:
            Target->Package =
                malloc(sizeof(AcpiPackage) + Source->Package->Size * sizeof(AcpiPackageElement));
            if (!Target->Package) {
                return 0;
            }

            Target->Package->References = 1;
            Target->Package->Size = Source->Package->Size;

            for (uint64_t i = 0; i < Source->Package->Size; i++) {
                if (Source->Package->Data[i].Type) {
                    if (!AcpiCopyValue(
                            &Source->Package->Data[i].Value, &Target->Package->Data[i].Value)) {
                        /* Recursive cleanup on failure (if we processed any items). */
                        Target->Package->Size = i ? i - 1 : 0;
                        AcpiRemoveReference(Target, 0);
                        return 0;
                    }
                } else {
                    memcpy(
                        &Target->Package->Data[i],
                        &Source->Package->Data[i],
                        sizeof(AcpiPackageElement));
                }
            }

            break;

        case ACPI_MUTEX:
            AcpipShowTraceMessage("attempt at CopyValue(Mutex)\n");
            return 0;

        case ACPI_FIELD_UNIT:
            AcpiCreateReference(&Source->FieldUnit.Region->Value, NULL);
            if (Source->FieldUnit.Data) {
                AcpiCreateReference(&Source->FieldUnit.Data->Value, NULL);
            }

            break;

        case ACPI_BUFFER_FIELD:
        case ACPI_INDEX:
            Source->BufferField.Source->References++;
            break;
    }

    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tells the object that either its containing pointer/object is being added
 *     inside another (and we need to increase the value reference counter), or that we're reusing
 *     its internal data (and just increasing the type-specific reference counter), according to
 *     Target.
 *
 * PARAMETERS:
 *     Source - Value being referenced.
 *     Target - Optional; Setting this to NULL means we want to reference the pointer/object
 *              itself, otherwise, it means we want to reference the inner data.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpiCreateReference(AcpiValue *Source, AcpiValue *Target) {
    if (!Source) {
        return;
    }

    if (!Target) {
        Source->References++;
        return;
    }

    memcpy(Target, Source, sizeof(AcpiValue));
    switch (Target->Type) {
        case ACPI_INTEGER:
            break;
        case ACPI_STRING:
            Target->String->References++;
            break;
        case ACPI_BUFFER:
            Target->Buffer->References++;
            break;
        case ACPI_PACKAGE:
            Target->Package->References++;
            break;
        case ACPI_FIELD_UNIT:
            AcpiCreateReference(&Target->FieldUnit.Region->Value, NULL);
            if (Target->FieldUnit.Data) {
                AcpiCreateReference(&Target->FieldUnit.Data->Value, NULL);
            }

            break;
        case ACPI_MUTEX:
            Target->Mutex->References++;
            break;
        case ACPI_BUFFER_FIELD:
        case ACPI_INDEX:
            AcpiCreateReference(Target->BufferField.Source, NULL);
            break;
        case ACPI_DEVICE:
        case ACPI_METHOD:
        case ACPI_REGION:
        case ACPI_POWER:
        case ACPI_PROCESSOR:
        case ACPI_THERMAL:
        case ACPI_SCOPE:
            Target->Children->References++;
            break;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function decreases the reference counter for the given value (and anything it
 *     references), freeing up any values that reach 0 references.
 *
 * PARAMETERS:
 *     Value - Pointer to the value to be freed.
 *     CleanupPointer - Set to 0 if this is a heap allocated pointer, or if you wish to reuse it
 *                      after freeing it.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpiRemoveReference(AcpiValue *Value, int CleanupPointer) {
    if (!Value || !Value->References) {
        return;
    }

    int NeedsCleanup = --Value->References <= 0;

    switch (Value->Type) {
        case ACPI_STRING:
            if (NeedsCleanup && --Value->String->References <= 0) {
                free(Value->String);
            }

            break;

        case ACPI_BUFFER:
            if (NeedsCleanup && --Value->Buffer->References <= 0) {
                free(Value->Buffer);
            }

            break;

        case ACPI_PACKAGE:
            if (NeedsCleanup && --Value->Package->References <= 0) {
                for (uint64_t i = 0; i < Value->Package->Size; i++) {
                    if (Value->Package->Data[i].Type) {
                        AcpiRemoveReference(&Value->Package->Data[i].Value, 0);
                    }
                }

                free(Value->Package);
            }

            break;

        case ACPI_FIELD_UNIT: {
            if (NeedsCleanup) {
                AcpiRemoveReference(&Value->FieldUnit.Region->Value, 0);
                if (Value->FieldUnit.Data) {
                    AcpiRemoveReference(&Value->FieldUnit.Data->Value, 0);
                }
            }

            break;
        }

        case ACPI_MUTEX: {
            if (NeedsCleanup && --Value->Mutex->References <= 0) {
                free(Value->Mutex);
            }

            break;
        }

        case ACPI_BUFFER_FIELD:
        case ACPI_INDEX:
            if (NeedsCleanup) {
                AcpiRemoveReference(Value->BufferField.Source, 1);
            }

            break;
    }

    if (NeedsCleanup && CleanupPointer) {
        free(Value);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates the required predefined root namespaces.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpipPopulatePredefined(void) {
#define PREDEFINED_ITEMS 5
    static const char Names[PREDEFINED_ITEMS][5] = {
        "_GPE",
        "_PR_",
        "_SB_",
        "_SI_",
        "_TZ_",
    };

    AcpiObject *Objects = calloc(PREDEFINED_ITEMS, sizeof(AcpiObject));
    if (!Objects) {
        AcpipShowErrorMessage(
            ACPI_REASON_OUT_OF_MEMORY, "could not allocate the predefined object scopes\n");
    }

    AcpiChildren *Children = calloc(PREDEFINED_ITEMS, sizeof(AcpiChildren));
    if (!Children) {
        AcpipShowErrorMessage(
            ACPI_REASON_OUT_OF_MEMORY, "could not allocate the predefined object scopes\n");
    }

    for (int i = 0; i < PREDEFINED_ITEMS; i++) {
        memcpy(Objects[i].Name, Names[i], 4);
        Objects[i].Value.Type = ACPI_SCOPE;
        Objects[i].Value.References = 1;
        Objects[i].Value.Children = &Children[i];
        Objects[i].Value.Children->References = 1;
        Objects[i].Value.Children->Objects = NULL;
        Objects[i].Next = NULL;
        Objects[i].Parent = &ObjectTreeRoot;

        if (i != PREDEFINED_ITEMS - 1) {
            Objects[i].Next = &Objects[i + 1];
        }
    }

    memcpy(ObjectTreeRoot.Name, "____", 4);
    ObjectTreeRoot.Value.Type = ACPI_SCOPE;
    ObjectTreeRoot.Value.References = 1;
    ObjectTreeRoot.Value.Children = &ObjectTreeRootChildren;
    ObjectTreeRootChildren.References = 1;
    ObjectTreeRootChildren.Objects = Objects;
    ObjectTreeRoot.Next = NULL;
    ObjectTreeRoot.Parent = NULL;
    AcpipObjectTree = &ObjectTreeRoot;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function populates the AML namespace tree using the given DSDT/SSDT.
 *
 * PARAMETERS:
 *     Code - Pointer to where the AML code starts.
 *     Length - Length of the code (without the SDT header!).
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpipPopulateTree(const uint8_t *Code, uint32_t Length) {
    AcpipState State;
    AcpipScope Scope;

    Scope.LinkedObject = AcpipObjectTree;
    Scope.Predicate = NULL;
    Scope.PredicateBacktrack = 0;
    Scope.Code = Code;
    Scope.Length = Length;
    Scope.RemainingLength = Length;
    Scope.Parent = NULL;

    memset(&State, 0, sizeof(State));
    State.IsMethod = 0;
    State.Scope = &Scope;
    State.Opcode = NULL;

    if (!AcpipExecuteTermList(&State)) {
        AcpipShowErrorMessage(ACPI_REASON_CORRUPTED_TABLES, "failed execution of ACPI table\n");
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function enters a new subscope.
 *
 * PARAMETERS:
 *     State - AML state containing the current scope.
 *     Name - Scope path (malloc'd or strdup'ed).
 *     Code - Scope entry point.
 *     Length - Size of the scope body.
 *     Root - Root object containing the scope/device object tree.
 *
 * RETURN VALUE:
 *     New state containing the subscope, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
AcpipScope *AcpipEnterScope(AcpipState *State, AcpiObject *Object, uint32_t Length) {
    AcpipScope *Scope = malloc(sizeof(AcpipScope));

    if (Scope) {
        Scope->LinkedObject = Object;
        Scope->Predicate = NULL;
        Scope->PredicateBacktrack = 0;
        Scope->Code = State->Scope->Code;
        Scope->Length = Length;
        Scope->RemainingLength = Length;
        Scope->Parent = State->Scope;
    }

    return Scope;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function enters a new If/Else scope.
 *
 * PARAMETERS:
 *     State - AML state containing the current scope.
 *     Length - Size of the body.
 *
 * RETURN VALUE:
 *     New state containing the scope, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
AcpipScope *AcpipEnterIf(AcpipState *State, uint32_t Length) {
    AcpipScope *Scope = malloc(sizeof(AcpipScope));

    if (Scope) {
        Scope->LinkedObject = State->Scope->LinkedObject;
        Scope->Predicate = NULL;
        Scope->PredicateBacktrack = 0;
        Scope->Code = State->Scope->Code;
        Scope->Length = Length;
        Scope->RemainingLength = Length;
        Scope->Parent = State->Scope;
    }

    return Scope;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function enters a new While scope.
 *
 * PARAMETERS:
 *     State - AML state containing the current scope.
 *     Predicate - Start position of the predicate (to be checked before each loop iteration).
 *     PredicateBacktrack - .RemainingPosition for the specified Predicate.
 *     Length - Size of the body.
 *
 * RETURN VALUE:
 *     New state containing the scope, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
AcpipScope *AcpipEnterWhile(
    AcpipState *State,
    const uint8_t *Predicate,
    uint32_t PredicateBacktrack,
    uint32_t Length) {
    AcpipScope *Scope = malloc(sizeof(AcpipScope));

    if (Scope) {
        Scope->LinkedObject = State->Scope->LinkedObject;
        Scope->Predicate = Predicate;
        Scope->PredicateBacktrack = PredicateBacktrack;
        Scope->Code = State->Scope->Code;
        Scope->Length = Length;
        Scope->RemainingLength = Length;
        Scope->Parent = State->Scope;
    }

    return Scope;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reads the next byte (8-bits) from the AML code stream, validating that we're
 *     still inside the code region.
 *
 * PARAMETERS:
 *     State - Current AML stream state.
 *     Byte - Output; What we've read, on success.
 *
 * RETURN VALUE:
 *     0 on end of code, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int AcpipReadByte(AcpipState *State, uint8_t *Byte) {
    if (State->Scope->RemainingLength) {
        *Byte = *(State->Scope->Code++);
        State->Scope->RemainingLength--;
        return 1;
    } else {
        return 0;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reads the next word (16-bits) from the AML code stream, validating that we're
 *     still inside the code region.
 *
 * PARAMETERS:
 *     State - Current AML stream state.
 *     Word - Output; What we've read, on success.
 *
 * RETURN VALUE:
 *     0 on end of code, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int AcpipReadWord(AcpipState *State, uint16_t *Word) {
    if (State->Scope->RemainingLength > 1) {
        *Word = *(uint16_t *)State->Scope->Code;
        State->Scope->Code += 2;
        State->Scope->RemainingLength -= 2;
        return 1;
    } else {
        return 0;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reads the next dword (32-bits) from the AML code stream, validating that
 *     we're still inside the code region.
 *
 * PARAMETERS:
 *     State - Current AML stream state.
 *     DWord - Output; What we've read, on success.
 *
 * RETURN VALUE:
 *     0 on end of code, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int AcpipReadDWord(AcpipState *State, uint32_t *DWord) {
    if (State->Scope->RemainingLength > 3) {
        *DWord = *(uint32_t *)State->Scope->Code;
        State->Scope->Code += 4;
        State->Scope->RemainingLength -= 4;
        return 1;
    } else {
        return 0;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reads the next qword (64-bits) from the AML code stream, validating that
 *     we're still inside the code region.
 *
 * PARAMETERS:
 *     State - Current AML stream state.
 *     QWord - Output; What we've read, on success.
 *
 * RETURN VALUE:
 *     0 on end of code, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int AcpipReadQWord(AcpipState *State, uint64_t *QWord) {
    if (State->Scope->RemainingLength > 7) {
        *QWord = *(uint64_t *)State->Scope->Code;
        State->Scope->Code += 8;
        State->Scope->RemainingLength -= 8;
        return 1;
    } else {
        return 0;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function parses a package length field.
 *
 * PARAMETERS:
 *     State - Current AML stream state.
 *     Length - Output; What we've read, on success.
 *
 * RETURN VALUE:
 *     0 on end of code, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int AcpipReadPkgLength(AcpipState *State, uint32_t *Length) {
    uint8_t Leading;
    if (!AcpipReadByte(State, &Leading)) {
        return 0;
    }

    /* High 2-bits of the leading byte specifies how many bytes we should read to get the package
       length; For 00, the other 6 bits are the package length itself, while for 01+, we only use
       the first 4 bits, followed by N whole bytes. */

    if (!(Leading >> 6)) {
        *Length = Leading & 0x3F;
        return 1;
    }

    *Length = Leading & 0x0F;

    for (int i = 0; i < Leading >> 6; i++) {
        uint8_t Part;
        if (!AcpipReadByte(State, &Part)) {
            return 0;
        }

        *Length |= (uint32_t)Part << (i * 8 + 4);
    }

    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function parses a name string, relative to the current scope.
 *
 * PARAMETERS:
 *     State - Current AML stream state.
 *     Name - Output; Will contain the name string data in case of a success.
 *
 * RETURN VALUE:
 *     0 on end of code, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int AcpipReadName(AcpipState *State, AcpiName *Name) {
    uint8_t Current;
    if (!AcpipReadByte(State, &Current)) {
        return 0;
    }

    int IsRoot = Current == '\\';
    int BacktrackCount = 0;

    /* Consume any and every `parent scope` prefixes, even if we don't have as many parent scopes
       and we're consuming. */
    if (IsRoot) {
        while (Current == '\\') {
            if (!AcpipReadByte(State, &Current)) {
                return 0;
            }
        }
    } else if (!IsRoot && Current == '^') {
        while (Current == '^') {
            if (!AcpipReadByte(State, &Current)) {
                return 0;
            }

            BacktrackCount++;
        }
    }

    /* The name itself is prefixed by a byte (or 2 for MultiNamePrefix) telling how many segments
       (4 characters each) we have. */
    uint8_t SegmentCount = 0;
    if (Current == 0x2E) {
        SegmentCount = 2;
    } else if (Current == 0x2F) {
        if (!AcpipReadByte(State, &SegmentCount)) {
            return 0;
        }
    } else if (Current) {
        SegmentCount = 1;
        State->Scope->RemainingLength++;
        State->Scope->Code--;
    }

    if (State->Scope->RemainingLength < SegmentCount * 4) {
        return 0;
    }

    Name->LinkedObject = IsRoot ? NULL : State->Scope->LinkedObject;
    Name->Start = State->Scope->Code;
    Name->BacktrackCount = BacktrackCount;
    Name->SegmentCount = SegmentCount;
    State->Scope->Code += SegmentCount * 4;
    State->Scope->RemainingLength -= SegmentCount * 4;

    return 1;
}
