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
    KeIrql Irql = HalpGetIrql();
    if (NewIrql < Irql) {
        KeFatalError(KE_WRONG_IRQL);
    }

    HalpSetIrql(NewIrql);
    return Irql;
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
    if (HalpGetIrql() < NewIrql) {
        KeFatalError(KE_WRONG_IRQL);
    }

    HalpSetIrql(NewIrql);
}
