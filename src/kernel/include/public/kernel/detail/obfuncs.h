/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_OBFUNCS_H_
#define _KERNEL_DETAIL_OBFUNCS_H_

#include <kernel/detail/obtypes.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, obfuncs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, obfuncs.h)
#endif /* __has__include */
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern ObTypeHeader ObThreadType;

void *ObCreateObject(ObTypeHeader *Type, char Tag[4]);
void ObReferenceObject(void *Body, char Tag[4]);
void ObDereferenceObject(void *Body, char Tag[4]);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _KERNEL_DETAIL_OBFUNCS_H_ */
