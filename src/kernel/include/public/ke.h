/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _KE_H_
#define _KE_H_

#include <stdint.h>

#define KE_FATAL_ERROR 0
#define KE_BAD_ACPI_TABLES 1
#define KE_BAD_POOL_HEADER 2
#define KE_DOUBLE_POOL_FREE 3
#define KE_DOUBLE_PAGE_FREE 4
#define KE_OUT_OF_MEMORY 5

#define KE_STACK_SIZE 0x4000

typedef int KeSpinLock;

void *KiFindAcpiTable(const char Signature[4], int Index);

int KeTryAcquireSpinLock(KeSpinLock *Lock);
void KeAcquireSpinLock(KeSpinLock *Lock);
void KeReleaseSpinLock(KeSpinLock *Lock);
int KeTestSpinLock(KeSpinLock *Lock);

[[noreturn]] void KeFatalError(int Message);

#endif /* _KE_H_ */
