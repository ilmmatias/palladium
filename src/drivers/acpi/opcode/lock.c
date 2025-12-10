/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <acpip.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries the execute the given opcode as a lock/mutex related opcode.
 *
 * PARAMETERS:
 *     State - AML state containing the current scope.
 *     Opcode - Opcode to be executed; The high bits contain the extended opcode number (when the
 *              lower opcode is 0x5B).
 *
 * RETURN VALUE:
 *     Positive number on success, negative on "this isn't a lock op", 0 on failure.
 *-----------------------------------------------------------------------------------------------*/
int AcpipExecuteLockOpcode(AcpipState *State, uint16_t Opcode, AcpiValue *Value) {
    (void)Value;

    switch (Opcode) {
        /* DefMutex := MutexOp NameString SyncFlags */
        case 0x015B: {
            AcpiValue MutexValue;
            MutexValue.Type = ACPI_MUTEX;
            MutexValue.References = 1;
            MutexValue.Mutex = AcpipAllocateBlock(sizeof(AcpiMutex));
            if (!MutexValue.Mutex) {
                return 0;
            }

            MutexValue.Mutex->References = 1;
            MutexValue.Mutex->SyncLevel = State->Opcode->FixedArguments[1].Integer & 0x0F;
            MutexValue.Mutex->Handle = AcpipCreateMutex();
            if (!MutexValue.Mutex->Handle) {
                AcpipFreeBlock(MutexValue.Mutex);
                return 0;
            }

            if (!AcpipCreateObject(&State->Opcode->FixedArguments[0].Name, &MutexValue)) {
                AcpipFreeBlock(MutexValue.Mutex);
                return 0;
            }

            break;
        }

        /* DefAcquire := AcquireOp MutexObject Timeout */
        case 0x235B: {
            AcpiValue MutexObject;
            if (!AcpipReadTarget(State, &State->Opcode->FixedArguments[0].TermArg, &MutexObject)) {
                return 0;
            }

            uint16_t Timeout = State->Opcode->FixedArguments[1].Integer;
            int Result = AcpipAcquireMutex(MutexObject.Mutex->Handle, Timeout);
            if (Value) {
                Value->Type = ACPI_INTEGER;
                Value->References = 1;
                Value->Integer = Result ? UINT64_MAX : 0;
            }

            AcpiRemoveReference(&State->Opcode->FixedArguments[0].TermArg, 0);
            break;
        }

        /* DefRelease := ReleaseOp MutexObject */
        case 0x275B: {
            AcpiValue MutexObject;
            if (!AcpipReadTarget(State, &State->Opcode->FixedArguments[0].TermArg, &MutexObject)) {
                return 0;
            }

            AcpipReleaseMutex(MutexObject.Mutex->Handle);

            AcpiRemoveReference(&State->Opcode->FixedArguments[0].TermArg, 0);
            break;
        }

        /* DefWait := WaitOp EventObject Operand */
        case 0x255B: {
            AcpiValue EventObject;
            if (!AcpipReadTarget(State, &State->Opcode->FixedArguments[0].TermArg, &EventObject)) {
                return 0;
            }

            uint64_t Timeout = State->Opcode->FixedArguments[1].TermArg.Integer;
            int Result = AcpipWaitEvent(EventObject.Event->Handle, Timeout);
            if (Value) {
                Value->Type = ACPI_INTEGER;
                Value->References = 1;
                Value->Integer = Result ? UINT64_MAX : 0;
            }

            AcpiRemoveReference(&State->Opcode->FixedArguments[0].TermArg, 0);
            AcpiRemoveReference(&State->Opcode->FixedArguments[1].TermArg, 0);
            break;
        }

        /* DefSignal := SignalOp EventObject */
        case 0x245B: {
            AcpiValue EventObject;
            if (!AcpipReadTarget(State, &State->Opcode->FixedArguments[0].TermArg, &EventObject)) {
                return 0;
            }

            AcpipSignalEvent(EventObject.Event->Handle);
            AcpiRemoveReference(&State->Opcode->FixedArguments[0].TermArg, 0);
            break;
        }

        /* DefReset := ResetOp EventObject */
        case 0x265B: {
            AcpiValue EventObject;
            if (!AcpipReadTarget(State, &State->Opcode->FixedArguments[0].TermArg, &EventObject)) {
                return 0;
            }

            AcpipResetEvent(EventObject.Event->Handle);
            AcpiRemoveReference(&State->Opcode->FixedArguments[0].TermArg, 0);
            break;
        }

        default:
            return -1;
    }

    return 1;
}
