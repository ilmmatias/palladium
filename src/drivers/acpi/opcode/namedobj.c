/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <stddef.h>
#include <stdlib.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries the execute the given opcode as a Named Object opcode.
 *     Differently from the `20.2.5.2. Named Objects Encoding` section in the AML spec, we don't
 *     parse field related ops here, but in ExecuteFieldOpcode.
 *
 * PARAMETERS:
 *     State - AML state containing the current scope.
 *     Opcode - Opcode to be executed; The high bits contain the extended opcode number (when the
 *              lower opcode is 0x5B).
 *
 * RETURN VALUE:
 *     Positive number on success, negative on "this isn't a named object", 0 on failure.
 *-----------------------------------------------------------------------------------------------*/
int AcpipExecuteNamedObjOpcode(AcpipState *State, uint16_t Opcode) {
    uint32_t Start = State->Scope->RemainingLength;

    switch (Opcode) {
        /* DefMethod := MethodOp PkgLength NameString MethodFlags TermList */
        case 0x14: {
            uint32_t Length;
            if (!AcpipReadPkgLength(State, &Length)) {
                return 0;
            }

            AcpipName Name;
            if (!AcpipReadName(State, &Name)) {
                return 0;
            }

            uint32_t LengthSoFar = Start - State->Scope->RemainingLength;
            if (LengthSoFar >= Length || Length - LengthSoFar > State->Scope->RemainingLength) {
                return 0;
            }

            AcpiValue Value;
            Value.Type = ACPI_METHOD;
            Value.References = 1;
            Value.Objects = NULL;
            Value.Method.Override = NULL;
            Value.Method.Start = State->Scope->Code + 1;
            Value.Method.Size = Length - LengthSoFar - 1;
            Value.Method.Flags = *State->Scope->Code;

            if (!AcpipCreateObject(&Name, &Value)) {
                return 0;
            }

            State->Scope->Code += Length - LengthSoFar;
            State->Scope->RemainingLength -= Length - LengthSoFar;
            break;
        }

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
            Value.Mutex.Flags = SyncFlags;

            if (!AcpipCreateObject(&Name, &Value)) {
                return 0;
            }

            break;
        }

        /* DefEvent := EventOp NameString */
        case 0x025B: {
            AcpipName Name;
            if (!AcpipReadName(State, &Name)) {
                return 0;
            }

            AcpiValue Value;
            Value.Type = ACPI_EVENT;
            Value.References = 1;

            if (!AcpipCreateObject(&Name, &Value)) {
                return 0;
            }

            break;
        }

        /* DefOpRegion := OpRegionOp NameString RegionSpace RegionOffset RegionLen */
        case 0x805B: {
            AcpipName Name;
            if (!AcpipReadName(State, &Name)) {
                return 0;
            }

            uint8_t RegionSpace;
            if (!AcpipReadByte(State, &RegionSpace)) {
                return 0;
            }

            uint64_t RegionOffset;
            if (!AcpipExecuteInteger(State, &RegionOffset)) {
                return 0;
            }

            uint64_t RegionLen;
            if (!AcpipExecuteInteger(State, &RegionLen)) {
                return 0;
            }

            AcpiValue Value;
            Value.Type = ACPI_REGION;
            Value.References = 1;
            Value.Objects = NULL;
            Value.Region.RegionSpace = RegionSpace;
            Value.Region.RegionLen = RegionLen;
            Value.Region.RegionOffset = RegionOffset;
            Value.Region.PciReady = 0;
            Value.Region.PciDevice = 0;
            Value.Region.PciFunction = 0;
            Value.Region.PciSegment = 0;
            Value.Region.PciBus = 0;

            if (!AcpipCreateObject(&Name, &Value)) {
                return 0;
            }

            break;
        }

        /* DefDevice := DeviceOp PkgLength NameString TermList
           DefThermalZone := ThermalZoneOp PkgLength NameString TermList */
        case 0x825B:
        case 0x855B: {
            uint32_t Length;
            if (!AcpipReadPkgLength(State, &Length)) {
                return 0;
            }

            AcpipName Name;
            if (!AcpipReadName(State, &Name)) {
                return 0;
            }

            uint32_t LengthSoFar = Start - State->Scope->RemainingLength;
            if (LengthSoFar > Length || Length - LengthSoFar > State->Scope->RemainingLength) {
                return 0;
            }

            AcpiValue Value;
            Value.Type = Opcode == 0x825B ? ACPI_DEVICE : ACPI_THERMAL;
            Value.References = 1;
            Value.Objects = NULL;

            AcpiObject *Object = AcpipCreateObject(&Name, &Value);
            if (!Object) {
                return 0;
            }

            AcpipScope *Scope = AcpipEnterScope(State, Object, Length - LengthSoFar);
            if (!Scope) {
                return 0;
            }

            State->Scope = Scope;
            break;
        }

        /* DefProcessor := ProcessorOp PkgLength NameString ProcID PblkAddr PblkLen TermList */
        case 0x835B: {
            uint32_t Length;
            if (!AcpipReadPkgLength(State, &Length)) {
                return 0;
            }

            AcpipName Name;
            if (!AcpipReadName(State, &Name)) {
                return 0;
            }

            uint32_t LengthSoFar = Start - State->Scope->RemainingLength + 6;
            if (LengthSoFar > Length || Length - LengthSoFar > State->Scope->RemainingLength) {
                return 0;
            }

            AcpiValue Value;
            Value.Type = ACPI_PROCESSOR;
            Value.References = 1;
            Value.Objects = NULL;
            Value.Processor.ProcId = *(State->Scope->Code);
            Value.Processor.PblkAddr = *(uint32_t *)(State->Scope->Code + 1);
            Value.Processor.PblkLen = *(State->Scope->Code + 5);

            State->Scope->Code += 6;
            State->Scope->RemainingLength -= 6;

            AcpiObject *Object = AcpipCreateObject(&Name, &Value);
            if (!Object) {
                return 0;
            }

            AcpipScope *Scope = AcpipEnterScope(State, Object, Length - LengthSoFar);
            if (!Scope) {
                return 0;
            }

            State->Scope = Scope;
            break;
        }

        /* DefPowerRes := PowerResOp PkgLength NameString SystemLevel ResourceOrder TermList */
        case 0x845B: {
            uint32_t Length;
            if (!AcpipReadPkgLength(State, &Length)) {
                return 0;
            }

            AcpipName Name;
            if (!AcpipReadName(State, &Name)) {
                return 0;
            }

            uint32_t LengthSoFar = Start - State->Scope->RemainingLength + 3;
            if (LengthSoFar > Length || Length - LengthSoFar > State->Scope->RemainingLength) {
                return 0;
            }

            AcpiValue Value;
            Value.Type = ACPI_POWER;
            Value.References = 1;
            Value.Objects = NULL;
            Value.Power.SystemLevel = *(State->Scope->Code);
            Value.Power.ResourceOrder = *(uint16_t *)(State->Scope->Code + 1);

            State->Scope->Code += 3;
            State->Scope->RemainingLength -= 3;

            AcpiObject *Object = AcpipCreateObject(&Name, &Value);
            if (!Object) {
                return 0;
            }

            AcpipScope *Scope = AcpipEnterScope(State, Object, Length - LengthSoFar);
            if (!Scope) {
                return 0;
            }

            State->Scope = Scope;
            break;
        }

        default:
            return -1;
    }

    return 1;
}
