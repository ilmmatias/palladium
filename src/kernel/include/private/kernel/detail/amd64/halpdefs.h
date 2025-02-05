/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_AMD64_HALPDEFS_H_
#define _KERNEL_DETAIL_AMD64_HALPDEFS_H_

#include <kernel/detail/amd64/halptypes.h>
#include <kernel/detail/amd64/kedefs.h>

#define HAL_INT_DISPATCH_IRQL KE_IRQL_DISPATCH
#define HAL_INT_DISPATCH_VECTOR (HAL_INT_DISPATCH_IRQL << 4)

#define HAL_INT_TIMER_IRQL (KE_IRQL_DEVICE + 10)
#define HAL_INT_TIMER_VECTOR (HAL_INT_TIMER_IRQL << 4)

#define HALP_PML4_LEVEL 0
#define HALP_PML4_BASE ((HalpPageFrame *)0xFFFFFFFFFFFFF000)
#define HALP_PML4_SHIFT 39
#define HALP_PML4_MASK 0x1FF
#define HALP_PML4_SIZE (1ull << HALP_PML4_SHIFT)

#define HALP_PDPT_LEVEL 1
#define HALP_PDPT_BASE ((HalpPageFrame *)0xFFFFFFFFFFE00000)
#define HALP_PDPT_SHIFT 30
#define HALP_PDPT_MASK 0x3FFFF
#define HALP_PDPT_SIZE (1ull << HALP_PDPT_SHIFT)

#define HALP_PD_LEVEL 2
#define HALP_PD_BASE ((HalpPageFrame *)0xFFFFFFFFC0000000)
#define HALP_PD_SHIFT 21
#define HALP_PD_MASK 0x7FFFFFF
#define HALP_PD_SIZE (1ull << HALP_PD_SHIFT)

#define HALP_PT_LEVEL 3
#define HALP_PT_BASE ((HalpPageFrame *)0xFFFFFF8000000000)
#define HALP_PT_SHIFT 12
#define HALP_PT_MASK 0xFFFFFFFFF
#define HALP_PT_SIZE (1ull << HALP_PT_SHIFT)

#endif /* _KERNEL_DETAIL_AMD64_HALPDEFS_H_ */
