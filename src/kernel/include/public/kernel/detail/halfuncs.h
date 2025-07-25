/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_HALFUNCS_H_
#define _KERNEL_DETAIL_HALFUNCS_H_

#include <kernel/detail/haltypes.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, halfuncs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, halfuncs.h)
#endif /* __has__include */
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

uint64_t HalGetTimerFrequency(void);
uint64_t HalGetTimerTicks(void);
void HalWaitTimer(uint64_t Time);

bool HalInitializeInterruptData(HalInterruptData *Data, uint32_t BusVector);
void HalAllocateInterruptVector(HalInterruptData *Data);
void HalReleaseInterruptData(HalInterruptData *Data);
HalInterrupt *HalCreateInterrupt(HalInterruptData *Data, void (*Handler)(void *), void *Context);
void HalDeleteInterrupt(HalInterrupt *Interrupt);
bool HalEnableInterrupt(HalInterrupt *Interrupt);
void HalDisableInterrupt(HalInterrupt *Interrupt);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _KERNEL_DETAIL_HALFUNCS_H_ */
