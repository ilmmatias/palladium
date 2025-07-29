/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_AMD64_KITYPES_H_
#define _KERNEL_DETAIL_AMD64_KITYPES_H_

#include <stdint.h>

typedef struct {
    uint64_t CycleCounterFrequency;
} KiLoaderArchData;

#endif /* _KERNEL_DETAIL_AMD64_KITYPES_H_ */
