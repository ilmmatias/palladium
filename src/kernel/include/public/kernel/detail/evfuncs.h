/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_EVFUNCS_H_
#define _KERNEL_DETAIL_EVFUNCS_H_

#include <kernel/detail/evtypes.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, evfuncs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, evfuncs.h)
#endif /* __has__include */
/* clang-format on */

EvSignal *EvCreateSignal(void);
void EvSetSignal(EvSignal *Signal);
void EvClearSignal(EvSignal *Signal);

bool EvWaitForObject(void *Object, uint64_t Timeout);

#endif /* _KERNEL_DETAIL_EVFUNCS_H_ */
