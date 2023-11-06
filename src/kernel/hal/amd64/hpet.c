/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <amd64/halp.h>
#include <amd64/hpet.h>
#include <ke.h>
#include <mm.h>
#include <vid.h>

static void *HpetAddress = NULL;
static uint64_t Period = 0;
static uint64_t PeriodInNs = 0;
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
    HpetHeader *Hpet = HalFindAcpiTable("HPET", 0);
    if (!Hpet) {
        VidPrint(KE_MESSAGE_ERROR, "Kernel HAL", "couldn't find the HPET table\n");
        KeFatalError(KE_BAD_ACPI_TABLES);
    }

    HpetAddress = MI_PADDR_TO_VADDR(Hpet->Address);

    uint64_t Caps = ReadHpetRegister(HPET_CAP_REG);
    Period = Caps >> 32;
    PeriodInNs = Period / 1000000;
    MinimumTicks = Hpet->MinimumTicks;
    VidPrint(
        KE_MESSAGE_INFO,
        "Kernel HAL",
        "using HPET as tick timer source (period = %llu ns)\n",
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
    return PeriodInNs;
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
