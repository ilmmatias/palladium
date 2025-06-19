/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_MMFUNCS_H_
#define _KERNEL_DETAIL_MMFUNCS_H_

#include <stddef.h>
#include <stdint.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, mmfuncs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, mmfuncs.h)
#endif /* __has__include */
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

uint64_t MmAllocateSinglePage();
void MmFreeSinglePage(uint64_t PhysicalAddress);

void *MmMapSpace(uint64_t PhysicalAddress, size_t Size, uint8_t Type);
void MmUnmapSpace(void *VirtualAddress);

void *MmAllocatePool(size_t Size, const char Tag[4]);
void MmFreePool(void *Base, const char Tag[4]);

void *MmAllocateKernelStack(void);
void MmFreeKernelStack(void *Base);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _KERNEL_DETAIL_MMFUNCS_H_ */
