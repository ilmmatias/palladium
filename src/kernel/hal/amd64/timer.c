/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <amd64/halp.h>
#include <vid.h>

extern void EvpHandleEvents(HalRegisterState *);

static uint64_t Period = 0;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the per-CPU event timer (Local APIC Timer).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpInitializeApicTimer(void) {
    HalpWriteLapicRegister(0x3E0, 0);

    /* We'll be taking the average over 4 runs. */
    uint64_t Accum = 0;
    uint64_t Ticks = (10 * EV_MILLISECS) / HalGetTimerPeriod();
    for (int i = 0; i < 4; i++) {
        uint64_t End = HalGetTimerTicks() + Ticks;
        HalpWriteLapicRegister(0x380, UINT32_MAX);
        while (HalGetTimerTicks() < End)
            ;

        HalpWriteLapicRegister(0x320, 0x10000);
        Accum += UINT32_MAX - HalpReadLapicRegister(0x390);
    }

    /* Install the vector (into the shared event handler) and enable interrupts (we should be
     * done). */
    if (!HalInstallInterruptHandlerAt(0x40, EvpHandleEvents, KE_IRQL_DISPATCH)) {
        VidPrint(VID_MESSAGE_ERROR, "Kernel HAL", "could not install the handler for the timer\n");
        KeFatalError(KE_FATAL_ERROR);
    }

    HalpWriteLapicRegister(0x320, 0x40);

    /* Round to closest, or we might be very off. */
    Period = (40000000 + Accum / 2) / Accum;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets up an event to execute in the specified amount of time.
 *
 * PARAMETERS:
 *     Time - How many microseconds to wait.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpSetEvent(uint64_t Time) {
    if (Time < Period) {
        Time = Period;
    }

    HalpWriteLapicRegister(0x3E0, 0);
    HalpWriteLapicRegister(0x380, Time / Period);
}
