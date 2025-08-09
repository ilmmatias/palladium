/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _OS_INTRIN_H_
#define _OS_INTRIN_H_

#include <stdint.h>

#if __has_include(ARCH_MAKE_INCLUDE_PATH(os, intrin.h))
#include ARCH_MAKE_INCLUDE_PATH(os, intrin.h)
#else
#error "Undefined ARCH for the SDK module!"
#endif

#endif /* _OS_INTRIN_H_ */
