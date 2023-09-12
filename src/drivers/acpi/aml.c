/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <ke.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static AcpiObject ObjectTreeRoot;
extern AcpiObject *AcpipObjectTree;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function starts execution of an AML method, based on a path.
 *
 * PARAMETERS:
 *     Name - Name attached to the method.
 *     ArgCount - Amount of arguments to pass.
 *     Arguments - Data of the arguments.
 *
 * RETURN VALUE:
 *     Return value of the method, or NULL if something went wrong.
 *-----------------------------------------------------------------------------------------------*/
AcpiValue *AcpiExecuteMethodFromPath(const char *Name, int ArgCount, AcpiValue *Arguments) {
    AcpiObject *Object = AcpiSearchObject(Name);
    return AcpiExecuteMethodFromObject(Object, ArgCount, Arguments);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function starts execution of an AML method, based on a previously searched object.
 *
 * PARAMETERS:
 *     Object - Object containing the method.
 *     ArgCount - Amount of arguments to pass.
 *     Arguments - Data of the arguments.
 *
 * RETURN VALUE:
 *     Return value of the method, or NULL if something went wrong.
 *-----------------------------------------------------------------------------------------------*/
AcpiValue *AcpiExecuteMethodFromObject(AcpiObject *Object, int ArgCount, AcpiValue *Arguments) {
    if (!Object || Object->Value.Type != ACPI_METHOD) {
        return NULL;
    } else if (ArgCount < 0) {
        ArgCount = 0;
    } else if (ArgCount > 7) {
        ArgCount = 7;
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

    if (ArgCount) {
        memcpy(State.Arguments, Arguments, ArgCount * sizeof(AcpiValue));
    }

    return AcpipExecuteTermList(&State);
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
    static const char Names[5][PREDEFINED_ITEMS] = {
        "_GPE",
        "_PR_",
        "_SB_",
        "_SI_",
        "_TZ_",
    };

    AcpiObject *Objects = calloc(PREDEFINED_ITEMS, sizeof(AcpiObject));
    if (!Objects) {
        KeFatalError(KE_EARLY_MEMORY_FAILURE);
    }

    for (int i = 0; i < PREDEFINED_ITEMS; i++) {
        memcpy(Objects[i].Name, Names[i], 4);
        Objects[i].Value.Type = ACPI_SCOPE;
        Objects[i].Parent = &ObjectTreeRoot;

        if (i != PREDEFINED_ITEMS - 1) {
            Objects[i].Next = &Objects[i + 1];
        }
    }

    memcpy(ObjectTreeRoot.Name, "____", 4);
    ObjectTreeRoot.Value.Type = ACPI_SCOPE;
    ObjectTreeRoot.Value.Objects = Objects;
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

    if (!AcpipExecuteTermList(&State)) {
        // KeFatalError(KE_CORRUPTED_HARDWARE_STRUCTURES);
        printf("Failure on AcpipExecuteTermList()!\n");
        while (1)
            ;
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

    uint8_t Part;
    *Length = Leading & 0x0F;

    int Shift = 4;
    switch (Leading >> 6) {
        case 0:
            *Length = Leading & 0x3F;
            break;
        case 3:
            if (!AcpipReadByte(State, &Part)) {
                return 0;
            }

            *Length |= (uint32_t)Part << Shift;
            Shift += 8;
        case 2:
            if (!AcpipReadByte(State, &Part)) {
                return 0;
            }

            *Length |= (uint32_t)Part << Shift;
            Shift += 8;
        case 1:
            if (!AcpipReadByte(State, &Part)) {
                return 0;
            }

            *Length |= (uint32_t)Part << Shift;
            Shift += 8;
    }

    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function parses a name string, relative to the current scope.
 *
 * PARAMETERS:
 *     State - Current AML stream state.
 *
 * RETURN VALUE:
 *     0 on end of code, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
AcpipName *AcpipReadName(AcpipState *State) {
    uint8_t Current;
    if (!AcpipReadByte(State, &Current)) {
        return NULL;
    }

    int IsRoot = Current == '\\';
    int BacktrackCount = 0;

    /* Consume any and every `parent scope` prefixes, even if we don't have as many parent scopes
       and we're consuming. */
    if (IsRoot && !AcpipReadByte(State, &Current)) {
        return NULL;
    } else if (!IsRoot && Current == '^') {
        while (Current == '^') {
            if (!AcpipReadByte(State, &Current)) {
                return NULL;
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
            return NULL;
        }
    } else if (Current) {
        SegmentCount = 1;
        State->Scope->RemainingLength++;
        State->Scope->Code--;
    }

    if (State->Scope->RemainingLength < SegmentCount * 4) {
        return NULL;
    }

    AcpipName *Name = malloc(sizeof(AcpipName));
    if (!Name) {
        return NULL;
    }

    Name->LinkedObject = IsRoot ? NULL : State->Scope->LinkedObject;
    Name->Start = State->Scope->Code;
    Name->BacktrackCount = BacktrackCount;
    Name->SegmentCount = SegmentCount;
    State->Scope->Code += SegmentCount * 4;
    State->Scope->RemainingLength -= SegmentCount * 4;

    return Name;
}
