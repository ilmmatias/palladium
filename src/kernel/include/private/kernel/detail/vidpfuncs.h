/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_VIDPFUNCS_H_
#define _KERNEL_DETAIL_VIDPFUNCS_H_

#include <kernel/detail/kitypes.h>
#include <kernel/detail/vidfuncs.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, vidpfuncs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, vidpfuncs.h)
#endif /* __has__include */
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void VidpInitialize(KiLoaderBlock *LoaderBlock);
void VidpAcquireOwnership(void);

KeIrql VidpAcquireSpinLock(void);
void VidpReleaseSpinLock(KeIrql OldIrql);

void VidpFlush(void);
void VidpPutChar(char Character);
void VidpPutString(const char *String);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _KERNEL_DETAIL_VIDPFUNCS_H_ */
