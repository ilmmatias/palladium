/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_AMD64_HALPDEFS_H_
#define _KERNEL_DETAIL_AMD64_HALPDEFS_H_

#include <kernel/detail/amd64/kedefs.h>

#define HAL_INT_DISPATCH_IRQL KE_IRQL_DISPATCH
#define HAL_INT_DISPATCH_VECTOR (HAL_INT_DISPATCH_IRQL << 4)

#define HAL_INT_TIMER_IRQL (KE_IRQL_DEVICE + 10)
#define HAL_INT_TIMER_VECTOR (HAL_INT_TIMER_IRQL << 4)

#endif /* _KERNEL_DETAIL_AMD64_HALPDEFS_H_ */
