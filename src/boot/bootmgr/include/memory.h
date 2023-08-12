/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _MEMORY_H_
#define _MEMORY_H_

#include <stddef.h>

#if defined(ARCH_x86) || defined(ARCH_amd64)
#define PAGE_SHIFT 12
#else
#error "Undefined ARCH for the bootmgr module!"
#endif /* ARCH */

#define PAGE_SIZE (1 << (PAGE_SHIFT))

void BmInitMemory(void *BootBlock);
void *BmAllocatePages(size_t Pages);
void BmFreePages(void *Base, size_t Pages);

#endif /* _MEMORY_H_ */
