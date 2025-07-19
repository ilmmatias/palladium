/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/detail/amd64/hpet.h>
#include <kernel/hal.h>
#include <kernel/ke.h>
#include <kernel/mm.h>
#include <kernel/vid.h>

static void *HpetAddress = NULL;
static uint64_t Frequency = 1;
static int Width = HAL_TIMER_WIDTH_64B;

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
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_HPET_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_TABLE_NOT_FOUND,
            0,
            0);
    }

    HpetAddress = MmMapSpace(Hpet->Address, MM_PAGE_SIZE, MM_SPACE_IO);
    if (!HpetAddress) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_HPET_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
            0,
            0);
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

    Reg = ReadHpetRegister(HPET_CAP_REG);
    Width = (Reg & HPET_CAP_64B) ? HAL_TIMER_WIDTH_64B : HAL_TIMER_WIDTH_32B;
    Frequency = 1000000000000000 / (Reg >> HPET_CAP_FREQ_START);

    /* Break down the frequency into something we can pretty print (in the smallest possible unit);
     * Using float/double would be good too, but we don't have floating point support in
     * __vprintf. */
    const char *Unit;
    uint64_t IntPart;
    uint64_t FractPart;
    if (Frequency >= 1000000000) {
        Unit = "GHz";
        IntPart = Frequency / 1000000000;
        FractPart = (Frequency % 1000000000) / 10000000;
    } else if (Frequency >= 1000000) {
        Unit = "MHz";
        IntPart = Frequency / 1000000;
        FractPart = (Frequency % 1000000) / 10000;
    } else if (Frequency >= 1000) {
        Unit = "KHz";
        IntPart = Frequency / 1000;
        FractPart = (Frequency % 1000) / 10;
    } else {
        Unit = "Hz";
        IntPart = Frequency;
        FractPart = 0;
    }

    VidPrint(
        VID_MESSAGE_INFO,
        "Kernel HAL",
        "using HPET as timer tick source (%u-bits counter, frequency = %llu.%02llu %s)\n",
        Width,
        IntPart,
        FractPart,
        Unit);

    /* At last we can reenable the main counter (after zeroing it). */
    Reg = ReadHpetRegister(HPET_CFG_REG);
    WriteHpetRegister(HPET_VAL_REG, 0);
    WriteHpetRegister(HPET_CFG_REG, (Reg & ~HPET_CFG_MASK) | HPET_CFG_INT_ENABLE);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function returns the width of the timer (useful for handling overflow!).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     HAL_TIMER_WIDTH_32B if the timer is 32-bits long, or HAL_TIMER_WIDTH_64B if the timer is
 *     64-bits long.
 *-----------------------------------------------------------------------------------------------*/
int HalGetTimerWidth(void) {
    return Width;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function returns the frequency (in HZ) of the HAL timer.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Frequency of the timer.
 *-----------------------------------------------------------------------------------------------*/
uint64_t HalGetTimerFrequency(void) {
    return Frequency;
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
