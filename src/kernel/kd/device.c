/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/kdp.h>
#include <kernel/ke.h>
#include <string.h>

KdpDebugDeviceDescriptor KdpDebugDevice = {};

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes and saves the data about a specific BAR location on the given
 *     PCI device.
 *
 * PARAMETERS:
 *     Bus - PCI bus number.
 *     Slot - PCI device number.
 *     Function - PCI function number.
 *     BarIndex - Which BAR location to initialize.
 *     PciHeader - PCI configuration header for the device.
 *
 * RETURN VALUE:
 *     How many locations we should skip (1 unless the BAR is for 64-bits MMIO, then it's 2).
 *-----------------------------------------------------------------------------------------------*/
static size_t InitializeBar(
    uint32_t Bus,
    uint32_t Device,
    uint32_t Function,
    size_t BarIndex,
    HalPciHeader *PciHeader) {
    /* All zeroes means this BAR isn't valid (and we can ignore it). */
    KdpDebugDeviceAddress *BaseAddress = &KdpDebugDevice.BaseAddress[BarIndex];
    uint32_t BarOffset = offsetof(HalPciHeader, Type0.BarAddress[BarIndex]);
    uint32_t BarAddress = PciHeader->Type0.BarAddress[BarIndex];
    size_t BarSize = 1;
    if (!BarAddress) {
        return BarSize;
    }

    /* Otherwise, start by grabbing up the basic data (we can do this without writing to any PCI
     * registers). */
    BaseAddress->Valid = true;

    uint64_t Address;
    if (BarAddress & 0x01) {
        BaseAddress->Type = KDP_RESOURCE_PORT;
        Address = BarAddress & ~0x03;
    } else {
        BaseAddress->Type = KDP_RESOURCE_MEMORY;
        Address = BarAddress & ~0x0F;
        if ((BarAddress & 0x06) == 0x04) {
            BaseAddress->BitWidth = 64;
            Address |= (uint64_t)PciHeader->Type0.BarAddress[BarIndex + 1] << 32;
            BarSize++;
        } else {
            BaseAddress->BitWidth = 32;
        }
    }

    /* Now we need to grab the size; This is a bit more annoying, and we're forced to write into the
     * BAR slot in the header, so we need to disable I/O and memory decoding (or we might cause some
     * unexpected problems). */
    uint32_t CmdOffset = offsetof(HalPciHeader, Command);
    uint16_t CmdWord = PciHeader->Command & ~0x03;
    HalWritePciConfigurationSpace(Bus, Device, Function, CmdOffset, &CmdWord, sizeof(uint16_t));

    /* The procedure to read the BAR size is somewhat simple: Write all ones to the BAR data, then
     * extract (NOT BAR_Value + 1), remembering to ignore any device flags in the start, and that's
     * our size; We just need to handle 32-bit vs 64-bit BARs for memory IO. */
    if (BaseAddress->BitWidth == 64) {
        uint64_t FullBarAddress;
        HalReadPciConfigurationSpace(
            Bus, Device, Function, BarOffset, &FullBarAddress, sizeof(uint64_t));

        uint64_t BarWord = UINT64_MAX;
        HalWritePciConfigurationSpace(Bus, Device, Function, BarOffset, &BarWord, sizeof(uint64_t));
        HalReadPciConfigurationSpace(Bus, Device, Function, BarOffset, &BarWord, sizeof(uint64_t));
        HalWritePciConfigurationSpace(
            Bus, Device, Function, BarOffset, &FullBarAddress, sizeof(uint64_t));

        BaseAddress->Length = ~(BarWord & ~0x0F) + 1;
    } else {
        uint32_t BarWord = UINT32_MAX;
        HalWritePciConfigurationSpace(Bus, Device, Function, BarOffset, &BarWord, sizeof(uint32_t));
        HalReadPciConfigurationSpace(Bus, Device, Function, BarOffset, &BarWord, sizeof(uint32_t));
        HalWritePciConfigurationSpace(
            Bus, Device, Function, BarOffset, &BarAddress, sizeof(uint32_t));

        if (BarAddress & 0x01) {
            BaseAddress->Length = ~(BarWord & ~0x03) + 1;
        } else {
            BaseAddress->Length = ~(BarWord & ~0x0F) + 1;
        }
    }

    /* Restore the I/O and memory decoding options; Also use the opportunity to enable bus
     * mastering. */
    CmdWord = PciHeader->Command | 0x07;
    HalWritePciConfigurationSpace(Bus, Device, Function, CmdOffset, &CmdWord, sizeof(uint16_t));

    /* For memory devices, we need to use the HAL to map in the device MMIO address. For ports, we
     * still need to add an API to translate the port into a valid "address" (mapping it or doing
     * whatever is necessary). */
    if (BarAddress & 0x01) {
        BaseAddress->TranslatedAddress = (uint8_t *)Address;
    } else {
        BaseAddress->TranslatedAddress =
            HalpMapEarlyMemory(Address, BaseAddress->Length, MI_MAP_WRITE | MI_MAP_UC);
        if (!BaseAddress->TranslatedAddress) {
            KeFatalError(
                KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_DEBUGGER_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
                0,
                0);
        }
    }

    return BarSize;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates a valid debug device descriptor for the extensibility module.
 *
 * PARAMETERS:
 *     LoaderBlock - Data prepared by the boot loader for us.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KdpInitializeDeviceDescriptor(KiLoaderBlock *LoaderBlock) {
    /* Shorter names for the debug data fields (to keep the lines a bit smaller). */
    uint32_t Segment = LoaderBlock->Debug.SegmentNumber;
    uint32_t Bus = LoaderBlock->Debug.BusNumber;
    uint32_t Device = LoaderBlock->Debug.DeviceNumber;
    uint32_t Function = LoaderBlock->Debug.FunctionNumber;

    /* We'll be extensively using the PCI config space header, so read it in. */
    HalPciHeader PciHeader;
    HalReadPciConfigurationSpace(Bus, Device, Function, 0, &PciHeader, sizeof(HalPciHeader));

    /* Most of the data from the descriptor can already be filled in, so let's do just that. */
    memset(&KdpDebugDevice, 0, sizeof(KdpDebugDeviceDescriptor));
    KdpDebugDevice.Bus = Bus;
    KdpDebugDevice.Slot = (Device & 0x1F) | ((Function & 0x07) << 5);
    KdpDebugDevice.Segment = Segment;
    KdpDebugDevice.VendorId = PciHeader.VendorId;
    KdpDebugDevice.DeviceId = PciHeader.DeviceId;
    KdpDebugDevice.BaseClass = PciHeader.Class;
    KdpDebugDevice.SubClass = PciHeader.SubClass;
    KdpDebugDevice.ProgIf = PciHeader.ProgIf;

    /* Now we're free to initialize and map the device BARs addresses (and wrap up by filling the
     * descriptor flags). */
    size_t BarIndex = 0;
    while (BarIndex < 6) {
        BarIndex += InitializeBar(Bus, Device, Function, BarIndex, &PciHeader);
    }

    KdpDebugDevice.Flags = KDP_DEVICE_FLAGS_BARS_MAPPED;
    KdpDebugDevice.Configured = true;
}
