/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _GENERIC_PROCESSOR_H_
#define _GENERIC_PROCESSOR_H_

#include <generic/lock.h>
#include <ps.h>

#ifdef ARCH_amd64
#include <amd64/processor.h>
#else
#error "Undefined ARCH for the kernel module!"
#endif /* ARCH */

#endif /* _GENERIC_PROCESSOR_H_ */
