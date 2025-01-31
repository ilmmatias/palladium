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

int HalGetTimerWidth(void);
uint64_t HalGetTimerPeriod(void);
uint64_t HalGetTimerTicks(void);
bool HalCheckTimerExpiration(uint64_t Current, uint64_t Reference, uint64_t Ticks);
void HalWaitTimer(uint64_t Time);

HalInterrupt *HalCreateInterrupt(
    KeIrql Irql,
    uint32_t Vector,
    uint8_t Type,
    void (*Handler)(HalInterruptFrame *, void *),
    void *HandlerContext);
bool HalEnableInterrupt(HalInterrupt *Interrupt);
void HalDisableInterrupt(HalInterrupt *Interrupt);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _KERNEL_DETAIL_HALFUNCS_H_ */
