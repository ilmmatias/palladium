/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _GENERIC_PAUSE_H_
#define _GENERIC_PAUSE_H_

#ifdef ARCH_amd64
#include <amd64/pause.h>
#else
#error "Undefined ARCH for the kernel module!"
#endif /* ARCH */

#endif /* _GENERIC_PAUSE_H_ */
