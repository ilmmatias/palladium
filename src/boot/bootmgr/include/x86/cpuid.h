/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _X86_CPUID_H_
#define _X86_CPUID_H_

#include <cpuid.h>

/* Extended Features (%eax == 0x80000001) */
/* %edx */
#define bit_PDPE1GB 0x4000000

#endif /* _X86_CPUID_H_ */
