/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

/* IWYU pragma: private, include <acpi.h> */

#ifndef _ACPI_DETAIL_ACPIFUNCS_H_
#define _ACPI_DETAIL_ACPIFUNCS_H_

#include <detail/acpitypes.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

AcpiObject *AcpiSearchObject(AcpiObject *Parent, const char *Name);
char *AcpiGetObjectPath(AcpiObject *Object);
bool AcpiEvaluateObject(AcpiObject *Object, AcpiValue *Result, int ExpectedType);
bool AcpiExecuteMethod(AcpiObject *Object, int ArgCount, AcpiValue *Arguments, AcpiValue *Result);

void AcpiCreateReference(AcpiValue *Source, AcpiValue *Target);
void AcpiRemoveReference(AcpiValue *Value, bool CleanupPointer);

bool AcpiCopyValue(AcpiValue *Source, AcpiValue *Target);
bool AcpiCastValue(AcpiValue *Source, AcpiValue *Target, int ExpectedType);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _ACPI_DETAIL_ACPIFUNCS_H_ */
