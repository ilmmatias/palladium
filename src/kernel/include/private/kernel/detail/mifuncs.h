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

void MiInitializeEarlyPageAllocator(KiLoaderBlock *LoaderBlock);
void MiInitializePageAllocator(void);
void MiReleaseBootRegions(void);
uint64_t MiAllocateEarlyPages(uint64_t Pages);

void MiInitializePool(void);
void *MiAllocatePoolPages(uint64_t Pages);
void MiFreePoolPages(void *Base);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _KERNEL_DETAIL_MIFUNCS_H_ */
