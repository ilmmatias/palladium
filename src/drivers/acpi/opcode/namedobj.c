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
    uint32_t Start = State->Opcode->Start;

    switch (Opcode) {
        /* DefMethod := MethodOp PkgLength NameString MethodFlags TermList */
        case 0x14: {
            uint32_t Length = State->Opcode->PkgLength;
            uint32_t LengthSoFar = Start - State->Scope->RemainingLength;
            if (LengthSoFar > Length || Length - LengthSoFar > State->Scope->RemainingLength) {
                return 0;
            }

            AcpiValue Value;
            Value.Type = ACPI_METHOD;
            Value.References = 1;
            Value.Children = malloc(sizeof(AcpiChildren));
            if (!Value.Children) {
                return 0;
            }

            Value.Children->References = 1;
            Value.Children->Objects = NULL;
            Value.Method.Override = NULL;
            Value.Method.Start = State->Scope->Code;
            Value.Method.Size = Length - LengthSoFar;
            Value.Method.Flags = State->Opcode->FixedArguments[1].Integer;

            if (!AcpipCreateObject(&State->Opcode->FixedArguments[0].Name, &Value)) {
                return 0;
            }

            State->Scope->Code += Length - LengthSoFar;
            State->Scope->RemainingLength -= Length - LengthSoFar;
            break;
        }

        /* DefEvent := EventOp NameString */
        case 0x025B: {
            AcpiValue Value;
            Value.Type = ACPI_EVENT;
            Value.References = 1;

            if (!AcpipCreateObject(&State->Opcode->FixedArguments[0].Name, &Value)) {
                return 0;
            }

            break;
        }

        /* DefOpRegion := OpRegionOp NameString RegionSpace RegionOffset RegionLen */
        case 0x805B: {
            AcpiValue Value;
            Value.Type = ACPI_REGION;
            Value.References = 1;
            Value.Children = malloc(sizeof(AcpiChildren));
            if (!Value.Children) {
                return 0;
            }

            Value.Children->References = 1;
            Value.Children->Objects = NULL;
            Value.Region.RegionSpace = State->Opcode->FixedArguments[1].Integer;
            Value.Region.RegionOffset = State->Opcode->FixedArguments[2].TermArg.Integer;
            Value.Region.RegionLen = State->Opcode->FixedArguments[3].TermArg.Integer;
            Value.Region.PciReady = 0;
            Value.Region.PciDevice = 0;
            Value.Region.PciFunction = 0;
            Value.Region.PciSegment = 0;
            Value.Region.PciBus = 0;

            if (!AcpipCreateObject(&State->Opcode->FixedArguments[0].Name, &Value)) {
                return 0;
            }

            break;
        }

        /* DefDevice := DeviceOp PkgLength NameString TermList
           DefThermalZone := ThermalZoneOp PkgLength NameString TermList */
        case 0x825B:
        case 0x855B: {
            uint32_t Length = State->Opcode->PkgLength;
            uint32_t LengthSoFar = Start - State->Scope->RemainingLength;
            if (LengthSoFar > Length || Length - LengthSoFar > State->Scope->RemainingLength) {
                return 0;
            }

            AcpiValue Value;
            Value.Type = Opcode == 0x825B ? ACPI_DEVICE : ACPI_THERMAL;
            Value.References = 1;
            Value.Children = malloc(sizeof(AcpiChildren));
            if (!Value.Children) {
                return 0;
            }

            Value.Children->References = 1;
            Value.Children->Objects = NULL;

            AcpiObject *Object = AcpipCreateObject(&State->Opcode->FixedArguments[0].Name, &Value);
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
            uint32_t Length = State->Opcode->PkgLength;
            uint32_t LengthSoFar = Start - State->Scope->RemainingLength;
            if (LengthSoFar > Length || Length - LengthSoFar > State->Scope->RemainingLength) {
                return 0;
            }

            AcpiValue Value;
            Value.Type = ACPI_PROCESSOR;
            Value.References = 1;
            Value.Children = malloc(sizeof(AcpiChildren));
            if (!Value.Children) {
                return 0;
            }

            Value.Children->References = 1;
            Value.Children->Objects = NULL;
            Value.Processor.ProcId = State->Opcode->FixedArguments[1].Integer;
            Value.Processor.PblkAddr = State->Opcode->FixedArguments[2].Integer;
            Value.Processor.PblkLen = State->Opcode->FixedArguments[3].Integer;

            AcpiObject *Object = AcpipCreateObject(&State->Opcode->FixedArguments[0].Name, &Value);
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
            uint32_t Length = State->Opcode->PkgLength;
            uint32_t LengthSoFar = Start - State->Scope->RemainingLength;
            if (LengthSoFar > Length || Length - LengthSoFar > State->Scope->RemainingLength) {
                return 0;
            }

            AcpiValue Value;
            Value.Type = ACPI_POWER;
            Value.References = 1;
            Value.Children = malloc(sizeof(AcpiChildren));
            if (!Value.Children) {
                return 0;
            }

            Value.Children->References = 1;
            Value.Children->Objects = NULL;
            Value.Power.SystemLevel = State->Opcode->FixedArguments[1].Integer;
            Value.Power.ResourceOrder = State->Opcode->FixedArguments[2].Integer;

            AcpiObject *Object = AcpipCreateObject(&State->Opcode->FixedArguments[0].Name, &Value);
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
