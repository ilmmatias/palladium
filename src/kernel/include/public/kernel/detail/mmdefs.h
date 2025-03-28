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
#define MM_POOL_TAG_EVENT "EVNT"

#endif /* _KERNEL_DETAIL_MMDEFS_H_ */
