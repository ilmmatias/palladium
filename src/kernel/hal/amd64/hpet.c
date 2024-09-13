/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <amd64/hpet.h>
#include <halp.h>
#include <ke.h>
#include <mi.h>
#include <vid.h>

static void *HpetAddress = NULL;
static uint64_t Period = 1;
static uint64_t MinimumTicks = 0;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reads the given HPET register.
 *
 * PARAMETERS:
 *     Number - Which register we want to read.
 *
 * RETURN VALUE:
 *     What we've read.
 *-----------------------------------------------------------------------------------------------*/
static uint64_t ReadHpetRegister(uint32_t Number) {
    return *(volatile uint64_t *)((char *)HpetAddress + Number);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function writes data into the given HPET register.
 *
 * PARAMETERS:
 *     Number - Which register we want to write into.
 *     Data - What we want to write.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void WriteHpetRegister(uint32_t Number, uint64_t Data) {
    *(volatile uint64_t *)((char *)HpetAddress + Number) = Data;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function finds and initializes the HPET (High Precision Event Time).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpInitializeHpet(void) {
    HpetHeader *Hpet = KiFindAcpiTable("HPET", 0);
    if (!Hpet) {
        VidPrint(VID_MESSAGE_ERROR, "Kernel HAL", "couldn't find the HPET table\n");
        KeFatalError(KE_BAD_ACPI_TABLES);
    }

    HpetAddress = MmMapSpace(Hpet->Address, MM_PAGE_SIZE);
    if (!HpetAddress) {
        VidPrint(VID_MESSAGE_ERROR, "Kernel HAL", "couldn't map the HPET table\n");
        KeFatalError(KE_BAD_ACPI_TABLES);
    }

    uint64_t Caps = ReadHpetRegister(HPET_CAP_REG);
    Period = (Caps >> 32) / 1000000;
    MinimumTicks = Hpet->MinimumTicks;
    VidPrint(
        VID_MESSAGE_DEBUG,
        "Kernel HAL",
        "using HPET as timer tick source (period = %llu ns)\n",
        Period);

    WriteHpetRegister(HPET_VAL_REG, 0);
    WriteHpetRegister(HPET_CFG_REG, 0x01);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function returns the period (in nanoseconds) of the HAL timer.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Period of the timer.
 *-----------------------------------------------------------------------------------------------*/
uint64_t HalGetTimerPeriod(void) {
    return Period;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function returns how many timer ticks have elapsed since the timer was initialized.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     How many ticks have elapsed since system boot. Multiply it by the timer period to get how
 *     many nanoseconds have elapsed.
 *-----------------------------------------------------------------------------------------------*/
uint64_t HalGetTimerTicks(void) {
    return ReadHpetRegister(HPET_VAL_REG);
}
