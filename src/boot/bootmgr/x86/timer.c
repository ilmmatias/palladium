/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <x86/timer.h>

static uint64_t StartValue = 0;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets up the RTC for the next wait operation.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BmSetupTimer(void) {
    WritePort(PORT_REG, 0x0A);
    while (ReadPort(PORT_DATA) & 0x80)
        ;

    WritePort(PORT_REG, 0x00);
    StartValue = ReadPort(PORT_DATA);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reads how many seconds have passed since the timer reset.
 *     We might roll over, so make sure to call BmSetupTimer asap (every 1 second if possible).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
uint64_t BmGetElapsedTime(void) {
    WritePort(PORT_REG, 0x0A);
    while (ReadPort(PORT_DATA) & 0x80)
        ;

    WritePort(PORT_REG, 0x00);
    return ReadPort(PORT_DATA) - StartValue;
}
