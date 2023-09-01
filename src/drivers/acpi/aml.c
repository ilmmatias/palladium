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
AcpipState *AcpipEnterMethod(AcpipState *State, char *Name, const uint8_t *Code, uint32_t Length) {
    AcpipState *MethodState = malloc(sizeof(AcpipState));

    if (MethodState) {
        MethodState->Scope = Name;
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
AcpipState *
AcpipEnterSubScope(AcpipState *State, char *Name, const uint8_t *Code, uint32_t Length) {
    AcpipState *ScopeState = malloc(sizeof(AcpipState));

    if (ScopeState) {
        ScopeState->Scope = Name;
        ScopeState->Code = Code;
        ScopeState->Length = Length;
        ScopeState->InMethod = 0;
        ScopeState->Parent = State;
    }

    return ScopeState;
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
