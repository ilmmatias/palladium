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

#define MI_PAGE_ENTRY(Base) (MiPageList[(uint64_t)(Base) >> MM_PAGE_SHIFT])
#define MI_PAGE_BASE(Entry) ((uint64_t)((Entry) - MiPageList) << MM_PAGE_SHIFT)

#define MI_PROCESSOR_PAGE_CACHE_HIGH_LIMIT 64
#define MI_PROCESSOR_PAGE_CACHE_LOW_LIMIT 16
#define MI_PROCESSOR_PAGE_CACHE_BATCH_SIZE 32

#endif /* _KERNEL_DETAIL_MIDEFS_H_ */
