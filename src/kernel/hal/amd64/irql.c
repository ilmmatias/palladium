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
KeIrql HalpGetIrql(void) {
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
void HalpSetIrql(KeIrql NewIrql) {
    __asm__ volatile("mov %0, %%cr8" : : "r"(NewIrql) : "memory");
}
