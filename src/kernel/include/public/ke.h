/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KE_H_
#define _KE_H_

#include <rt/list.h>

#ifdef ARCH_amd64
#include <amd64/processor.h>
#else
#error "Undefined ARCH for the kernel module!"
#endif /* ARCH */

#define KE_EVENT_NONE 0
#define KE_EVENT_FREEZE 1

#define KE_PANIC_MANUALLY_INITIATED_CRASH 0
#define KE_PANIC_IRQL_NOT_LESS_OR_EQUAL 1
#define KE_PANIC_IRQL_NOT_GREATER_OR_EQUAL 2
#define KE_PANIC_IRQL_NOT_DISPATCH 3
#define KE_PANIC_SPIN_LOCK_ALREADY_OWNED 4
#define KE_PANIC_SPIN_LOCK_NOT_OWNED 5
#define KE_PANIC_EXCEPTION_NOT_HANDLED 6
#define KE_PANIC_TRAP_NOT_HANDLED 7
#define KE_PANIC_PAGE_FAULT_NOT_HANDLED 8
#define KE_PANIC_SYSTEM_SERVICE_NOT_HANDLED 9
#define KE_PANIC_NMI_HARDWARE_FAILURE 10
#define KE_PANIC_INSTALL_MORE_MEMORY 11
#define KE_PANIC_BAD_PFN_HEADER 12
#define KE_PANIC_BAD_POOL_HEADER 13
#define KE_PANIC_BAD_SYSTEM_TABLE 14
#define KE_PANIC_COUNT 15

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

[[noreturn]] void KeFatalError(uint32_t Message);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _KE_H_ */
