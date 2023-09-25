/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <stdio.h>
#include <stdlib.h>

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
            AcpipName Name;
            if (!AcpipReadName(State, &Name)) {
                return 0;
            }

            uint8_t SyncFlags;
            if (!AcpipReadByte(State, &SyncFlags)) {
                return 0;
            }

            AcpiValue Value;
            Value.Type = ACPI_MUTEX;
            Value.References = 1;
            Value.Mutex = malloc(sizeof(AcpiMutex));
            if (!Value.Mutex) {
                return 0;
            }

            Value.Mutex->References = 1;
            Value.Mutex->Flags = SyncFlags;
            atomic_flag_clear(&Value.Mutex->Value);

            if (!AcpipCreateObject(&Name, &Value)) {
                return 0;
            }

            break;
        }

        /* DefAcquire := AcquireOp MutexObject Timeout */
        case 0x235B: {
            AcpipTarget MutexName;
            if (!AcpipExecuteSuperName(State, &MutexName, 0)) {
                return 0;
            }

            AcpiValue MutexObject;
            if (!AcpipReadTarget(State, &MutexName, &MutexObject)) {
                return 0;
            }

            uint16_t Timeout;
            if (!AcpipReadWord(State, &Timeout)) {
                return 0;
            }

            /* STUB, we should handle the timeout + use some kind of thread blocking fn
               after we implement threads in the kernel. */
            while (atomic_flag_test_and_set(&MutexObject.Mutex->Value))
                ;

            break;
        }

        /* DefRelease := ReleaseOp MutexObject */
        case 0x275B: {
            AcpipTarget MutexName;
            if (!AcpipExecuteSuperName(State, &MutexName, 0)) {
                return 0;
            }

            AcpiValue MutexObject;
            if (!AcpipReadTarget(State, &MutexName, &MutexObject)) {
                return 0;
            }

            /* STUB, send a "mutex release" signal to the kernel after we implement threading. */
            atomic_flag_clear(&MutexObject.Mutex->Value);
            break;
        }

        default:
            return -1;
    }

    return 1;
}
