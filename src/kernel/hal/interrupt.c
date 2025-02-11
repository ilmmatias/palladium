/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/hal.h>
#include <kernel/mm.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates and initializes a new interrupt object, getting it ready for
 *     HalEnableInterrupt.
 *
 * PARAMETERS:
 *     Irql - Target IRQL of the interrupt handler.
 *     Vector - Target vector of the interrupt; The exact meaning of this varies by platform.
 *     Type - Type of the interrupt (level or edge).
 *     Handler - Function to be called when the interrupt gets triggered.
 *     HandlerContext - Data to be provided to the interrupt handler when it gets triggered.
 *
 * RETURN VALUE:
 *     Either the interrupt object, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
HalInterrupt *HalCreateInterrupt(
    KeIrql Irql,
    uint32_t Vector,
    uint8_t Type,
    void (*Handler)(HalInterruptFrame *, void *),
    void *HandlerContext) {
    HalInterrupt *Interrupt = MmAllocatePool(sizeof(HalInterrupt), MM_POOL_TAG_INTERRUPT);
    if (!Interrupt) {
        return NULL;
    }

    Interrupt->Enabled = false;
    Interrupt->Lock = 0;
    Interrupt->Irql = Irql;
    Interrupt->Vector = Vector;
    Interrupt->Type = Type;
    Interrupt->Handler = Handler;
    Interrupt->HandlerContext = HandlerContext;
    return Interrupt;
}
