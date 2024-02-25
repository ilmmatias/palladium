/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KE_H_
#define _KE_H_

#include <rt/list.h>

#ifdef ARCH_amd64
#include <amd64/processor.h>
#else
#error "Undefined ARCH for the kernel module!"
#endif /* ARCH */

#define KE_NO_EVENT 0
#define KE_PANIC_EVENT 1

#define KE_FATAL_ERROR 0
#define KE_BAD_ACPI_TABLES 1
#define KE_BAD_POOL_HEADER 2
#define KE_DOUBLE_POOL_FREE 3
#define KE_DOUBLE_PAGE_FREE 4
#define KE_OUT_OF_MEMORY 5
#define KE_WRONG_IRQL 6
#define KE_PANIC_COUNT 7

#if defined(ARCH_amd64)
#define KE_IRQL_PASSIVE 0
#define KE_IRQL_DISPATCH 4
#define KE_IRQL_DEVICE 5
#define KE_IRQL_CLOCK 14
#define KE_IRQL_MASK 15

#define KE_STACK_SIZE 0x2000
#else
#error "Undefined ARCH for the kernel module!"
#endif /* ARCH */

typedef int KeSpinLock;
typedef uintptr_t KeIrql;

typedef struct {
    RtDList ListHeader;
    void *ImageBase;
    void *EntryPoint;
    uint32_t SizeOfImage;
    const char *ImageName;
} KeModule;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern RtDList KeModuleListHead;

void *KiFindAcpiTable(const char Signature[4], int Index);

KeIrql KeGetIrql(void);
KeIrql KeRaiseIrql(KeIrql NewIrql);
void KeLowerIrql(KeIrql NewIrql);

int KeTryAcquireSpinLock(KeSpinLock *Lock);
KeIrql KeAcquireSpinLock(KeSpinLock *Lock);
void KeReleaseSpinLock(KeSpinLock *Lock, KeIrql NewIrql);
int KeTestSpinLock(KeSpinLock *Lock);

[[noreturn]] void KeFatalError(int Message);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _KE_H_ */
