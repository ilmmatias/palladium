/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_KIFUNCS_H_
#define _KERNEL_DETAIL_KIFUNCS_H_

#include <kernel/detail/kefuncs.h>
#include <kernel/detail/kitypes.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, kifuncs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, kifuncs.h)
#endif /* __has__include */
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void KiSaveAcpiData(KiLoaderBlock *LoaderBlock);

void KiSaveBootStartDrivers(KiLoaderBlock *BootData);
void KiRunBootStartDrivers(void);
void KiDumpSymbol(void *Address);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _KERNEL_DETAIL_KIFUNCS_H_ */
