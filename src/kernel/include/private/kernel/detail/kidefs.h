/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_KIDEFS_H_
#define _KERNEL_DETAIL_KIDEFS_H_

#include <kernel/detail/kedefs.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, kidefs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, kidefs.h)
#endif /* __has__include */
/* clang-format on */

#define KI_ACPI_NONE 0
#define KI_ACPI_RDST 1
#define KI_ACPI_XSDT 2

#endif /* _KERNEL_DETAIL_KIDEFS_H_ */
