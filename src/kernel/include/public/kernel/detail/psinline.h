/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_PSINLINE_H_
#define _KERNEL_DETAIL_PSINLINE_H_

#include <kernel/detail/keinline.h>
#include <kernel/detail/pstypes.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, psinline.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, psinline.h)
#endif /* __has__include */
/* clang-format on */

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function gets a pointer to the thread struct of the current running thread.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Pointer to the thread struct.
 *-----------------------------------------------------------------------------------------------*/
static inline PsThread *PsGetCurrentThread(void) {
    return KeGetCurrentProcessor()->CurrentThread;
}

#endif /* _KERNEL_DETAIL_PSINLINE_H_ */
