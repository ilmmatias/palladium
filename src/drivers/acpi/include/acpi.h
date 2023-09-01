/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _ACPI_H_
#define _ACPI_H_

#include <stdint.h>

#define ACPI_OBJECT_INTEGER 0

typedef struct {
    int Type;
    union {
        uint64_t Integer;
    } Value;
} AcpiObject;

typedef struct AcpipState {
    char *Scope;
    const uint8_t *Code;
    uint32_t Length;
    uint32_t RemainingLength;
    int InMethod;
    struct AcpipState *Parent;
} AcpipState;

void AcpipInitializeFromRsdt(void);
void AcpipInitializeFromXsdt(void);

void AcpipPopulateTree(const uint8_t *Code, uint32_t Length);
AcpipState *AcpipEnterMethod(AcpipState *State, char *Name, const uint8_t *Code, uint32_t Length);
AcpipState *AcpipEnterSubScope(AcpipState *State, char *Name, const uint8_t *Code, uint32_t Length);

int AcpipReadByte(AcpipState *State, uint8_t *Byte);
int AcpipReadWord(AcpipState *State, uint16_t *Word);
int AcpipReadDWord(AcpipState *State, uint32_t *DWord);
int AcpipReadQWord(AcpipState *State, uint64_t *QWord);

AcpiObject *AcpipExecuteTermList(AcpipState *State);

#endif /* _ACPI_H_ */
