/* SPDX-FileCopyrightText: (C) 2023-2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

/* IWYU pragma: private, include <acpip.h> */

#ifndef _ACPIP_DETAIL_ACPIPTYPES_H_
#define _ACPIP_DETAIL_ACPIPTYPES_H_

#include <detail/acpitypes.h>

typedef struct {
    bool Valid;
    int Count;
    bool HasPkgLength;
    int Types[6];
} AcpipArgument;

typedef union {
    uint64_t Integer;
    AcpiString *String;
    AcpiName Name;
    AcpiValue TermArg;
} AcpipArgumentValue;

typedef struct AcpipOpcode {
    const uint8_t *StartCode;
    uint32_t Start;
    const uint8_t *Predicate;
    uint32_t PredicateBacktrack;
    uint16_t Opcode;
    AcpipArgument *ArgInfo;
    bool ValidPkgLength;
    uint32_t PkgLength;
    int ValidArgs;
    AcpipArgumentValue FixedArguments[6];
    AcpipArgumentValue *VariableArguments;
    int ParentArgType;
    AcpipArgumentValue *ParentArg;
    struct AcpipOpcode *Parent;
} AcpipOpcode;

typedef struct AcpipScope {
    AcpiObject *LinkedObject;
    const uint8_t *Predicate;
    uint32_t PredicateBacktrack;
    const uint8_t *Code;
    uint32_t Length;
    uint32_t RemainingLength;
    struct AcpipScope *Parent;
} AcpipScope;

typedef struct AcpipState {
    bool IsMethod;
    bool HasReturned;
    AcpiValue Arguments[7];
    AcpiValue Locals[8];
    AcpiValue ReturnValue;
    AcpipScope *Scope;
    AcpipOpcode *Opcode;
} AcpipState;

#endif /* _ACPIP_DETAIL_ACPIPTYPES_H_ */
