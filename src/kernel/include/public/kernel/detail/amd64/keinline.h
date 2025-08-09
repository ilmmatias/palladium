/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_AMD64_KEINLINE_H_
#define _KERNEL_DETAIL_AMD64_KEINLINE_H_

#include <kernel/detail/amd64/ketypes.h>
#include <os/amd64/intrin.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function gets the current interrupt level.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Current IRQL level.
 *-----------------------------------------------------------------------------------------------*/
static inline KeIrql KeGetIrql(void) {
    KeIrql Irql;
    __asm__ volatile("mov %%cr8, %0" : "=r"(Irql) : : "memory");
    return Irql;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function forcefully sets the current IRQL level, you should only use this if you
 *     REALLY know what you're doing, or you WILL break something.
 *
 * PARAMETERS:
 *     NewIrql - Target IRQL level.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static inline void KeSetIrql(KeIrql NewIrql) {
    __asm__ volatile("mov %0, %%cr8" : : "r"(NewIrql) : "memory");
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function gets a pointer to the processor-specific structure of the current processor.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Pointer to the processor struct.
 *-----------------------------------------------------------------------------------------------*/
static inline KeProcessor *KeGetCurrentProcessor(void) {
    return (KeProcessor *)ReadMsr(0xC0000102);
}

#endif /* _KERNEL_DETAIL_AMD64_KEINLINE_H_ */
