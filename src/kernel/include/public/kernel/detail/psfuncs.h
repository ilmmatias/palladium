/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_PSFUNCS_H_
#define _KERNEL_DETAIL_PSFUNCS_H_

#include <kernel/detail/pstypes.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, psfuncs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, psfuncs.h)
#endif /* __has__include */
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

PsThread *PsCreateThread(uint64_t Flags, void (*EntryPoint)(void *), void *Parameter);
[[noreturn]] void PsTerminateThread(void);
void PsYieldThread(void);
void PsDelayThread(uint64_t Time);

void PsInitializeAlert(PsAlert *Alert, void (*Routine)(void *), void *Context);
bool PsQueueAlert(PsThread *Thread, PsAlert *Alert);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _KERNEL_DETAIL_PSFUNCS_H_ */
