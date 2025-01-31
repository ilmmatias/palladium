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

PsThread *PsCreateThread(void (*EntryPoint)(void *), void *Parameter);
void PsQueueThread(PsThread *Thread);
[[noreturn]] void PsTerminateThread(void);
void PsYieldThread(int Type);
void PsDelayThread(uint64_t Time);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _KERNEL_DETAIL_PSFUNCS_H_ */
