/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _RT_CONTEXT_H_
#define _RT_CONTEXT_H_

#include <stdint.h>

#if __has_include(ARCH_MAKE_INCLUDE_PATH(rt, context.h))
#include ARCH_MAKE_INCLUDE_PATH(rt, context.h)
#else
#error "Undefined ARCH for the CRT module!"
#endif /* __has_include */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void RtSaveContext(RtContext *Context);
[[noreturn]] void RtRestoreContext(RtContext *Context);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _RT_EXCEPT_H_ */
