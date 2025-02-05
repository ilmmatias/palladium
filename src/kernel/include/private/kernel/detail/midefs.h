/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_MIDEFS_H_
#define _KERNEL_DETAIL_MIDEFS_H_

#include <kernel/detail/mmdefs.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, midefs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, midefs.h)
#endif /* __has__include */
/* clang-format on */

#define MI_MAP_WRITE 0x01
#define MI_MAP_EXEC 0x02
#define MI_MAP_WC 0x04
#define MI_MAP_UC 0x08

#define MI_DESCR_FREE 0x00
#define MI_DESCR_PAGE_MAP 0x01
#define MI_DESCR_LOADED_PROGRAM 0x02
#define MI_DESCR_GRAPHICS_BUFFER 0x03
#define MI_DESCR_OSLOADER_TEMPORARY 0x04
#define MI_DESCR_FIRMWARE_TEMPORARY 0x05
#define MI_DESCR_FIRMWARE_PERMANENT 0x06
#define MI_DESCR_SYSTEM_RESERVED 0x07

#define MI_PAGE_FLAGS_USED 0x01
#define MI_PAGE_FLAGS_CONTIG_BASE 0x02
#define MI_PAGE_FLAGS_CONTIG_ITEM 0x04
#define MI_PAGE_FLAGS_CONTIG_ANY (MI_PAGE_FLAGS_CONTIG_BASE | MI_PAGE_FLAGS_CONTIG_ITEM)
#define MI_PAGE_FLAGS_POOL_BASE 0x08
#define MI_PAGE_FLAGS_POOL_ITEM 0x10
#define MI_PAGE_FLAGS_POOL_ANY (MI_PAGE_FLAGS_POOL_BASE | MI_PAGE_FLAGS_POOL_ITEM)

#define MI_PAGE_ENTRY(Base) (MiPageList[(uint64_t)(Base) >> MM_PAGE_SHIFT])
#define MI_PAGE_BASE(Entry) ((uint64_t)((Entry) - MiPageList) << MM_PAGE_SHIFT)

#endif /* _KERNEL_DETAIL_MIDEFS_H_ */
