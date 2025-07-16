/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_MIFUNCS_H_
#define _KERNEL_DETAIL_MIFUNCS_H_

#include <kernel/detail/kitypes.h>
#include <kernel/detail/mitypes.h>
#include <kernel/detail/mmfuncs.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, mifuncs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, mifuncs.h)
#endif /* __has__include */
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern MiPageEntry *MiPageList;
extern uint64_t MiTotalSystemPages;
extern uint64_t MiTotalReservedPages;
extern uint64_t MiTotalCachedPages;
extern uint64_t MiTotalUsedPages;
extern uint64_t MiTotalFreePages;
extern uint64_t MiTotalBootPages;
extern uint64_t MiTotalGraphicsPages;
extern uint64_t MiTotalPtePages;
extern uint64_t MiTotalPfnPages;
extern uint64_t MiTotalPoolPages;
extern RtAtomicSList MiPoolTagListHead[256];

void MiInitializeEarlyPageAllocator(KiLoaderBlock *LoaderBlock);
void MiInitializePageAllocator(void);
void MiReleaseBootRegions(void);
uint64_t MiAllocateEarlyPages(uint32_t Pages);

void MiInitializePool(void);
void *MiAllocatePoolPages(uint32_t Pages);
uint32_t MiFreePoolPages(void *Base);

void MiInitializePoolTracker(void);
uint8_t MiGetTagHash(const char Tag[4]);
MiPoolTrackerHeader *MiFindTracker(const char Tag[4]);
void MiAddPoolTracker(size_t Size, const char Tag[4]);
void MiRemovePoolTracker(size_t Size, const char Tag[4]);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _KERNEL_DETAIL_MIFUNCS_H_ */
