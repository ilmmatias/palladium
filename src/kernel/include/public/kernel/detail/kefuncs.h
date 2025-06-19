/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_KEFUNCS_H_
#define _KERNEL_DETAIL_KEFUNCS_H_

#include <kernel/detail/ketypes.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, kefuncs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, kefuncs.h)
#endif /* __has__include */
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void *KiFindAcpiTable(const char Signature[4], int Index);

void KeInitializeDpc(KeDpc *Dpc, void (*Routine)(void *), void *Context);
void KeQueueDpc(KeDpc *Dpc, bool HighPriority);

void KeInitializeAffinity(KeAffinity *Mask);
bool KeGetAffinityBit(KeAffinity *Mask, uint32_t Index);
void KeSetAffinityBit(KeAffinity *Mask, uint32_t Index);
void KeClearAffinityBit(KeAffinity *Mask, uint32_t Index);
uint32_t KeGetFirstAffinitySetBit(KeAffinity *Mask);
uint32_t KeGetFirstAffinityClearBit(KeAffinity *Mask);
uint64_t KeCountAffinitySetBits(KeAffinity *Mask);
uint64_t KeCountAffinityClearBits(KeAffinity *Mask);

void KeSynchronizeProcessors(volatile uint64_t *State);
void KeRequestIpiRoutine(void (*Routine)(void *), void *Parameter);

[[noreturn]] void KeFatalError(
    uint32_t Message,
    uint64_t Parameter1,
    uint64_t Parameter2,
    uint64_t Parameter3,
    uint64_t Parameter4);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _KERNEL_DETAIL_KEFUNCS_H_ */
