/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_MIFUNCS_H_
#define _KERNEL_DETAIL_MIFUNCS_H_

#include <kernel/detail/kitypes.h>
#include <kernel/detail/mmfuncs.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, mifuncs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, mifuncs.h)
#endif /* __has__include */
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void MiInitializePageAllocator(KiLoaderBlock *LoaderBlock);
void MiInitializePool(KiLoaderBlock *LoaderBlock);
void MiReleaseBootRegions(void);

void *MiEnsureEarlySpace(uint64_t PhysicalAddress, size_t Size);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _KERNEL_DETAIL_MIFUNCS_H_ */
