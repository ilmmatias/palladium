/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_PSPFUNCS_H_
#define _KERNEL_DETAIL_PSPFUNCS_H_

#include <kernel/detail/psfuncs.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, pspfuncs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, pspfuncs.h)
#endif /* __has__include */
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

[[noreturn]] void PspInitializeScheduler(void);
void PspCreateIdleThread(void);
void PspCreateSystemThread(void);

void PspQueueThread(PsThread *Thread, bool EventQueue);
void PspQueueThreadList(RtDList *ThreadList, uint64_t ThreadCount, bool EventQueue);
void PspSetupThreadWait(KeProcessor *Processor, PsThread *Thread, uint64_t Time);
void PspSuspendExecution(
    KeProcessor *Processor,
    PsThread *CurrentThread,
    int NewState,
    KeIrql OldIrql);
void PspSwitchThreads(
    KeProcessor *Processor,
    PsThread *CurrentThread,
    PsThread *TargetThread,
    uint8_t Type,
    KeIrql OldIrql);

RtAvlCompareResult PspCompareWaitThreads(RtAvlNode *FirstStruct, RtAvlNode *SecondStruct);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _KERNEL_DETAIL_PSPFUNCS_H_ */
