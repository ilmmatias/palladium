/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _MM_H_
#define _MM_H_

#include <stddef.h>
#include <stdint.h>

#ifdef ARCH_amd64
#define MM_PAGE_SHIFT 12
#else
#error "Undefined ARCH for the kernel module!"
#endif /* ARCH */

#define MM_PAGE_SIZE (1ull << (MM_PAGE_SHIFT))

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

uint64_t MmAllocateSinglePage();
void MmFreeSinglePage(uint64_t PhysicalAddress);

void *MmMapSpace(uint64_t PhysicalAddress, size_t Size);
void MmUnmapSpace(void *VirtualAddress);

void *MmAllocatePool(size_t Size, const char Tag[4]);
void MmFreePool(void *Base, const char Tag[4]);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _MM_H_ */
