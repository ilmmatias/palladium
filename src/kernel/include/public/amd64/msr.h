/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _AMD64_MSR_H_
#define _AMD64_MSR_H_

#include <stdint.h>

/* Wrappers around rd/wrmsr functions. */

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reads the specified MSR leaf.
 *
 * PARAMETERS:
 *     Number - Which leaf we should read.
 *
 * RETURN VALUE:
 *     What RDMSR returned.
 *-----------------------------------------------------------------------------------------------*/
static inline uint64_t ReadMsr(uint32_t Number) {
    uint32_t LowPart;
    uint32_t HighPart;
    __asm__ volatile("rdmsr" : "=a"(LowPart), "=d"(HighPart) : "c"(Number));
    return LowPart | ((uint64_t)HighPart << 32);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function writes into the specified MSR leaf.
 *
 * PARAMETERS:
 *     Number - Which leaf we should write into.
 *     Value - What we want to write.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static inline void WriteMsr(uint32_t Number, uint64_t Value) {
    __asm__ volatile("wrmsr" : : "a"((uint32_t)Value), "c"(Number), "d"(Value >> 32));
}

#endif /* _AMD64_MSR_H_ */
