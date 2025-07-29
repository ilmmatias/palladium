/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/detail/amd64/hpet.h>
#include <kernel/halp.h>
#include <kernel/ke.h>
#include <kernel/mm.h>
#include <kernel/vid.h>

extern bool HalpTimerInitialized;
extern bool HalpTscActive;

static void *HpetAddress = NULL;
static uint64_t Frequency = 1;
static int Width = 64;

static HpetOverflowHelper OverflowHelper = {};

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
 *     This function handles a clock event (updating the overflow stats for 32-bit HPET counters).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpHandleTimer(void) {
    /* This routine should only run in the BSP, and only for active (in use) 32-bit HPET timers
     * (don't do anything if the TSC is active instead). */
    if (Width != 32 || HalpTscActive || KeGetCurrentProcessor()->Number) {
        return;
    }

    /* Compare the current HPET value with the previous one; This should be enough to determine if
     * an overflow happend. */
    uint64_t CurrentValue = ReadHpetRegister(HPET_VAL_REG);
    HpetOverflowHelper NewValue;
    if (CurrentValue < OverflowHelper.LowPart) {
        NewValue.HighPart = OverflowHelper.HighPart + 1;

    } else {
        NewValue.HighPart = OverflowHelper.HighPart;
    }

    NewValue.LowPart = CurrentValue;
    __atomic_store_n(&OverflowHelper.RawData, NewValue.RawData, __ATOMIC_RELEASE);
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
    HpetHeader *Hpet = HalpFindEarlyAcpiTable("HPET");
    if (!Hpet) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_HPET_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_TABLE_NOT_FOUND,
            0,
            0);
    }

    HpetAddress = HalpMapEarlyMemory(Hpet->Address, MM_PAGE_SIZE, MI_MAP_WRITE | MI_MAP_UC);
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
    Width = (Reg & HPET_CAP_64B) ? 64 : 32;
    Frequency = 1000000000000000 / (Reg >> HPET_CAP_FREQ_START);

    /* At last we can reenable the main counter (after zeroing it). */
    Reg = ReadHpetRegister(HPET_CFG_REG);
    WriteHpetRegister(HPET_VAL_REG, 0);
    WriteHpetRegister(HPET_CFG_REG, (Reg & ~HPET_CFG_MASK) | HPET_CFG_INT_ENABLE);

    /* The TSC calibration wants a good/precise timer if it can't use CPUID, so set us up as the
     * active timer for now. */
    HalpSetActiveTimer(Frequency, HalpGetHpetTicks);
    VidPrint(
        VID_MESSAGE_DEBUG,
        "Kernel HAL",
        "found a %d-bits HPET (frequency = %llu.%02llu MHz)\n",
        Width,
        Frequency / 1000000,
        (Frequency % 1000000) / 10000);

    HalpUnmapEarlyMemory(Hpet, Hpet->Length);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function returns the frequency (in HZ) of the HPET.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Frequency of the timer.
 *-----------------------------------------------------------------------------------------------*/
uint64_t HalpGetHpetFrequency(void) {
    return Frequency;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function returns how many timer ticks have elapsed since the HPET was initialized.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     How many ticks have elapsed since system boot. Multiply it by the timer period to get how
 *     many nanoseconds have elapsed.
 *-----------------------------------------------------------------------------------------------*/
uint64_t HalpGetHpetTicks(void) {
    /* For 64-bit timers, don't bother with the overflow code (overflows are only guaranteed to be
     * handled for 32-bit timers); We also use the same path for any waits before the timer
     * subsystem is fully initailized, as the LAPIC timer (and overflow detection) isn't online yet
     * in that case. */
    if (Width == 64 || !HalpTimerInitialized) {
        return ReadHpetRegister(HPET_VAL_REG);
    }

    /* Otherwise, let's cooperate with the overflow handler to try and fix our high part. */
    HpetOverflowHelper OldValue = {
        .RawData = __atomic_load_n(&OverflowHelper.RawData, __ATOMIC_ACQUIRE)};
    uint64_t LowPart = ReadHpetRegister(HPET_VAL_REG);
    if (LowPart < OldValue.LowPart) {
        return ((uint64_t)(OldValue.HighPart + 1) << 32) | LowPart;
    } else {
        return ((uint64_t)OldValue.HighPart << 32) | LowPart;
    }
}
