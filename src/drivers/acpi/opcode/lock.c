/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <stdio.h>

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
            AcpiValue Value;
            Value.Type = ACPI_MUTEX;
            Value.References = 1;
            Value.Mutex = AcpipAllocateBlock(sizeof(AcpiMutex));
            if (!Value.Mutex) {
                return 0;
            }

            Value.Mutex->References = 1;
            Value.Mutex->Flags = State->Opcode->FixedArguments[1].Integer;
            atomic_flag_clear(&Value.Mutex->Value);

            if (!AcpipCreateObject(&State->Opcode->FixedArguments[0].Name, &Value)) {
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

            /* STUB, we should handle the timeout + use some kind of thread blocking fn
               after we implement threads in the kernel. */
            while (atomic_flag_test_and_set(&MutexObject.Mutex->Value))
                ;

            AcpiRemoveReference(&State->Opcode->FixedArguments[0].TermArg, 0);
            break;
        }

        /* DefRelease := ReleaseOp MutexObject */
        case 0x275B: {
            AcpiValue MutexObject;
            if (!AcpipReadTarget(State, &State->Opcode->FixedArguments[0].TermArg, &MutexObject)) {
                return 0;
            }

            /* STUB, send a "mutex release" signal to the kernel after we implement threading. */
            atomic_flag_clear(&MutexObject.Mutex->Value);

            AcpiRemoveReference(&State->Opcode->FixedArguments[0].TermArg, 0);
            break;
        }

        default:
            return -1;
    }

    return 1;
}
