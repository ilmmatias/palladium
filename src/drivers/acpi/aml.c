/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    State.Scope = strdup("\\");
    State.ScopeSegs = 0;
    State.Code = Code;
    State.Length = Length;
    State.RemainingLength = Length;
    State.InMethod = 0;
    State.Parent = NULL;

    if (!State.Scope) {
        return;
    }

    printf("AcpipPopulateTree(%p, %u)\n", Code, Length);
    AcpipExecuteTermList(&State);
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
 *
 * RETURN VALUE:
 *     New state containing the subscope, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
AcpipState *AcpipEnterSubScope(AcpipState *State, char *Name, uint8_t NameSegs, uint32_t Length) {
    AcpipState *ScopeState = malloc(sizeof(AcpipState));

    if (ScopeState) {
        ScopeState->Scope = Name;
        ScopeState->ScopeSegs = NameSegs;
        ScopeState->Code = State->Code;
        ScopeState->Length = Length;
        ScopeState->RemainingLength = Length;
        ScopeState->InMethod = 0;
        ScopeState->Parent = State;
    }

    return ScopeState;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function enters a new method scope.
 *
 * PARAMETERS:
 *     State - AML state containing the current scope.
 *     Name - Method name (malloc'd or strdup'ed).
 *     Code - Method entry point.
 *     Length - Size of the method body.
 *
 * RETURN VALUE:
 *     New state containing the method scope, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
AcpipState *AcpipEnterMethod(
    AcpipState *State,
    char *Name,
    uint8_t NameSegs,
    const uint8_t *Code,
    uint32_t Length) {
    AcpipState *MethodState = malloc(sizeof(AcpipState));

    if (MethodState) {
        MethodState->Scope = Name;
        MethodState->ScopeSegs = NameSegs;
        MethodState->Code = Code;
        MethodState->Length = Length;
        MethodState->RemainingLength = Length;
        MethodState->InMethod = 1;
        MethodState->Parent = State;
    }

    return MethodState;
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
    if (State->RemainingLength) {
        *Byte = *(State->Code++);
        State->RemainingLength--;
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
    if (State->RemainingLength > 1) {
        *Word = *(uint16_t *)State->Code;
        State->Code += 2;
        State->RemainingLength -= 2;
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
    if (State->RemainingLength > 3) {
        *DWord = *(uint32_t *)State->Code;
        State->Code += 4;
        State->RemainingLength -= 4;
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
    if (State->RemainingLength > 7) {
        *QWord = *(uint64_t *)State->Code;
        State->Code += 8;
        State->RemainingLength -= 8;
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

    switch (Leading >> 6) {
        case 0:
            *Length = Leading & 0x3F;
            break;
        case 3:
            if (!AcpipReadByte(State, &Part)) {
                return 0;
            }

            *Length |= (uint32_t)Part << 20;
        case 2:
            if (!AcpipReadByte(State, &Part)) {
                return 0;
            }

            *Length |= (uint32_t)Part << 12;
        case 1:
            if (!AcpipReadByte(State, &Part)) {
                return 0;
            }

            *Length |= (uint32_t)Part << 4;
    }

    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function parses a name string, relative to the current scope.
 *
 * PARAMETERS:
 *     State - Current AML stream state.
 *     NameString - Output; What we've read, on success.
 *     NameSegs - Output; How many segments the name has.
 *
 * RETURN VALUE:
 *     0 on end of code, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int AcpipReadNameString(AcpipState *State, char **NameString, uint8_t *NameSegs) {
    uint8_t Current;
    if (!AcpipReadByte(State, &Current)) {
        return 0;
    }

    int IsRoot = Current == '\\';
    int Prefixes = 0;

    /* Consume any and every `parent scope` prefixes, even if we don't have as many parent scopes
       and we're consuming. */
    if (IsRoot && !AcpipReadByte(State, &Current)) {
        return 0;
    } else if (!IsRoot && Current == '^') {
        while (Current == '^') {
            if (!AcpipReadByte(State, &Current)) {
                return 0;
            }

            Prefixes++;
        }
    }

    /* Follow up by making sure we're not going to far/exceeding the root scope. */
    if (Prefixes > State->ScopeSegs) {
        Prefixes = State->ScopeSegs;
    }

    /* The name itself is prefixed by a byte (or 2 for MultiNamePrefix) telling how many segments
       (4 characters each) we have. */
    *NameSegs = 0;

    if (Current == 0x2E) {
        *NameSegs = 2;
    } else if (Current == 0x2F) {
        if (!AcpipReadByte(State, NameSegs)) {
            return 0;
        }
    } else if (Current) {
        *NameSegs = 1;
        State->RemainingLength++;
        State->Code--;
    }

    if (State->RemainingLength < *NameSegs * 4) {
        return 0;
    }

    /* If we're directly inside the root scope, our path will be `\<name segs>`, otherwise, it'll
       be `\<scope>.<name segs>` */
    uint32_t ParentScopeBytes = 1;
    uint32_t ChildScopeBytes;

    /* Space for the root scope + prefix scopes (and +1 for each dot separating the childs). */
    if (!IsRoot && (State->ScopeSegs - Prefixes)) {
        ParentScopeBytes = (State->ScopeSegs - Prefixes) * 5 + 1;
    }

    /* Space for the name segments (+1 for the dots). */
    if (*NameSegs > 1) {
        ChildScopeBytes = *NameSegs * 5;
    } else {
        ChildScopeBytes = *NameSegs * 4;
    }

    *NameString = calloc(ParentScopeBytes + ChildScopeBytes + 1, 1);
    if (!*NameString) {
        return 0;
    }

    int NamePos = 0;

    if (IsRoot || !(State->ScopeSegs - Prefixes)) {
        (*NameString)[NamePos++] = '\\';
    } else {
        memcpy(*NameString, State->Scope, ParentScopeBytes - 1);
        NamePos = ParentScopeBytes - 1;
        (*NameString)[NamePos++] = '.';
    }

    for (uint8_t i = 0; i < *NameSegs; i++) {
        if (i) {
            (*NameString)[NamePos++] = '.';
        }

        memcpy(*NameString + NamePos, State->Code, 4);

        NamePos += 4;
        State->Code += 4;
        State->RemainingLength -= 4;
    }

    if (!IsRoot) {
        *NameSegs += State->ScopeSegs - Prefixes;
    }

    return 1;
}
