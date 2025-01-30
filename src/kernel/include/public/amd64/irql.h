/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _AMD64_IRQL_H_
#define _AMD64_IRQL_H_

#include <generic/panic.h>
#include <stdint.h>

#define KE_IRQL_PASSIVE 0
#define KE_IRQL_DISPATCH 2
#define KE_IRQL_DEVICE 3
#define KE_IRQL_SYNCH 13
#define KE_IRQL_IPI 14
#define KE_IRQL_MAX 15

typedef volatile uint64_t KeIrql;

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
 *     This function raises the current interrupt level; This will panic if a value lower than
 *     the current is passed.
 *
 * PARAMETERS:
 *     NewIrql - Target level.
 *
 * RETURN VALUE:
 *     Old IRQL level.
 *-----------------------------------------------------------------------------------------------*/
static inline KeIrql KeRaiseIrql(KeIrql NewIrql) {
    KeIrql OldIrql = KeGetIrql();
    if (NewIrql < OldIrql) {
        KeFatalError(KE_PANIC_IRQL_NOT_GREATER_OR_EQUAL, OldIrql, NewIrql, 0, 0);
    }

    KeSetIrql(NewIrql);
    return OldIrql;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function lowers the current interrupt level; This will panic if a value higher than
 *     the current is passed.
 *
 * PARAMETERS:
 *     NewIrql - Target level.
 *
 * RETURN VALUE:
 *     Old IRQL level.
 *-----------------------------------------------------------------------------------------------*/
static inline void KeLowerIrql(KeIrql NewIrql) {
    KeIrql OldIrql = KeGetIrql();
    if (OldIrql < NewIrql) {
        KeFatalError(KE_PANIC_IRQL_NOT_LESS_OR_EQUAL, OldIrql, NewIrql, 0, 0);
    }

    KeSetIrql(NewIrql);
}

#endif /* _AMD64_IRQL_H_ */
