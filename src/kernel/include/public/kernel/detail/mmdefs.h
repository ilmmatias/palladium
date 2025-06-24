/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_MMDEFS_H_
#define _KERNEL_DETAIL_MMDEFS_H_

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, mmdefs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, mmdefs.h)
#endif /* __has__include */
/* clang-format on */

#define MM_PAGE_SIZE (1ull << (MM_PAGE_SHIFT))

#define MM_SPACE_NORMAL 0
#define MM_SPACE_IO 1
#define MM_SPACE_GRAPHICS 2

#define MM_POOL_TAG_ACPI "ACPI"
#define MM_POOL_TAG_APIC "APIC"
#define MM_POOL_TAG_INTERRUPT "INTR"
#define MM_POOL_TAG_LDR "LDR "
#define MM_POOL_TAG_OBJECT "OBJ "
#define MM_POOL_TAG_PFN "PFN "
#define MM_POOL_TAG_POOL "POOL"
#define MM_POOL_TAG_PROCESS "PCB "
#define MM_POOL_TAG_PROCESSOR "PCR "
#define MM_POOL_TAG_THREAD "TCB "
#define MM_POOL_TAG_KERNEL_STACK "KSTK"
#define MM_POOL_TAG_EVENT "EVNT"

/* This is only required to be defined here instead of midefs.h becase ketypes.h uses it. */
#define MM_POOL_SMALL_SHIFT (4)
#define MM_POOL_SMALL_MAX (MM_PAGE_SIZE >> 2)
#define MM_POOL_SMALL_COUNT (MM_POOL_SMALL_MAX >> MM_POOL_SMALL_SHIFT)
#define MM_POOL_SMALL_PAGES (1)

#define MM_POOL_MEDIUM_SHIFT (5)
#define MM_POOL_MEDIUM_MIN (MM_POOL_SMALL_MAX)
#define MM_POOL_MEDIUM_MAX (MM_PAGE_SIZE >> 1)
#define MM_POOL_MEDIUM_COUNT ((MM_POOL_MEDIUM_MAX - MM_POOL_MEDIUM_MIN) >> MM_POOL_MEDIUM_SHIFT)
#define MM_POOL_MEDIUM_PAGES (2)

#define MM_POOL_LARGE_SHIFT (6)
#define MM_POOL_LARGE_MIN (MM_POOL_MEDIUM_MAX)
#define MM_POOL_LARGE_MAX (MM_PAGE_SIZE - 16)
#define MM_POOL_LARGE_COUNT ((MM_POOL_LARGE_MAX - MM_POOL_LARGE_MIN) >> MM_POOL_LARGE_SHIFT)
#define MM_POOL_LARGE_PAGES (4)

#define MM_POOL_BLOCK_COUNT (MM_POOL_SMALL_COUNT + MM_POOL_MEDIUM_COUNT + MM_POOL_LARGE_COUNT)

#endif /* _KERNEL_DETAIL_MMDEFS_H_ */
