/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <amd64/hpet.h>
#include <halp.h>
#include <ke.h>
#include <mi.h>
#include <vid.h>

static void *HpetAddress = NULL;
static uint64_t Period = 1;

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
        KeFatalError(KE_PANIC_BAD_SYSTEM_TABLE);
    }

    HpetAddress = MmMapSpace(Hpet->Address, MM_PAGE_SIZE);
    if (!HpetAddress) {
        VidPrint(VID_MESSAGE_ERROR, "Kernel HAL", "couldn't map the HPET table\n");
        KeFatalError(KE_PANIC_INSTALL_MORE_MEMORY);
    }

    /* We can't/shouldn't be messing with the HPET registers if the counter (and interrupts) are
     * enabled. */
    uint64_t Reg = ReadHpetRegister(HPET_CFG_REG);
    WriteHpetRegister(HPET_CFG_REG, Reg & ~HPET_CFG_MASK);

    /* We'll just assume that the firmware left the HPET comparators in a state that would be bad to
     * us, and reset all comparators. */
    for (uint8_t i = 0; i < Hpet->ComparatorCount + 1; i++) {
        Reg = ReadHpetRegister(HPET_TIMER_CAP_REG(i));
        WriteHpetRegister(HPET_TIMER_CAP_REG(i), Reg & ~HPET_TIMER_MASK);
    }

    Period = (ReadHpetRegister(HPET_CAP_REG) >> 32) / 1000000;
    VidPrint(
        VID_MESSAGE_DEBUG,
        "Kernel HAL",
        "using HPET as timer tick source (period = %llu ns)\n",
        Period);

    /* At last we can reenable the main counter (after zeroing it). */
    Reg = ReadHpetRegister(HPET_CFG_REG);
    WriteHpetRegister(HPET_VAL_REG, 0);
    WriteHpetRegister(HPET_CFG_REG, (Reg & ~HPET_CFG_MASK) | HPET_CFG_INT_ENABLE);
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
