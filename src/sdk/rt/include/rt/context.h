/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _RT_CONTEXT_H_
#define _RT_CONTEXT_H_

#include <stdint.h>

#if defined(ARCH_x86) || defined(ARCH_amd64)
#include <rt/amd64/context.h>
#else
#error "Undefined ARCH for the rt module!"
#endif

void RtSaveContext(RtContext *Context);
[[noreturn]] void RtRestoreContext(RtContext *Context);

#endif /* _RT_EXCEPT_H_ */
