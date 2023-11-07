/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <amd64/halp.h>
#include <vid.h>

extern void PspHandleEvent(HalRegisterState *);

static uint64_t Period = 0;
static uint8_t Irq = 0;

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
    uint64_t Ticks = (10 * HAL_MILLISECS) / HalGetTimerPeriod();
    for (int i = 0; i < 4; i++) {
        uint64_t End = HalGetTimerTicks() + Ticks;
        HalpWriteLapicRegister(0x380, UINT32_MAX);
        while (HalGetTimerTicks() < End)
            ;

        HalpWriteLapicRegister(0x320, 0x10000);
        Accum += UINT32_MAX - HalpReadLapicRegister(0x390);
    }

    /* Allocate a vector and enable interrupts (and we're done). */
    Irq = HalInstallInterruptHandler(PspHandleEvent);
    if (Irq == (uint8_t)-1) {
        VidPrint(
            VID_MESSAGE_ERROR, "Kernel HAL", "could not allocate an IRQ for the event timer\n");
        KeFatalError(KE_FATAL_ERROR);
    }

    HalpWriteLapicRegister(0x320, Irq + 32);

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
