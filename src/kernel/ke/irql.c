/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp.h>

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
KeIrql KeGetIrql(void) {
    return HalpGetIrql();
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
KeIrql KeRaiseIrql(KeIrql NewIrql) {
    KeIrql OldIrql = HalpGetIrql();
    if (NewIrql < OldIrql) {
        KeFatalError(KE_PANIC_IRQL_NOT_GREATER_OR_EQUAL, OldIrql, NewIrql, 0, 0);
    }

    HalpSetIrql(NewIrql);
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
void KeLowerIrql(KeIrql NewIrql) {
    KeIrql OldIrql = HalpGetIrql();
    if (OldIrql < NewIrql) {
        KeFatalError(KE_PANIC_IRQL_NOT_LESS_OR_EQUAL, OldIrql, NewIrql, 0, 0);
    }

    HalpSetIrql(NewIrql);
}
