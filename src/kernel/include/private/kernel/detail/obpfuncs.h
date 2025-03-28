/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_OBPFUNCS_H_
#define _KERNEL_DETAIL_OBPFUNCS_H_

#include <kernel/detail/obfuncs.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, obpfuncs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, obpfuncs.h)
#endif /* __has__include */
/* clang-format on */

extern ObType ObpDirectoryType;
extern ObType ObpMutexType;
extern ObType ObpSignalType;
extern ObType ObpThreadType;

void ObpInitializeRootDirectory(void);

#endif /* _KERNEL_DETAIL_OBPFUNCS_H_ */
