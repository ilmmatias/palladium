/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/kdp.h>
#include <kernel/ke.h>
#include <kernel/mi.h>
#include <string.h>

extern KdpDebugDeviceDescriptor KdpDebugDevice;
extern KdpExtensibilityImports KdpDebugImports;
extern uint32_t KdpDebugErrorStatus;
extern uint16_t *KdpDebugErrorString;
extern uint32_t KdpDebugHardwareId;

bool KdpDebugEnabled = false;
bool KdpDebugConnected = false;
void *KdpDebugAdapter = NULL;
KdpExtensibilityData KdpDebugData = {};

uint8_t KdpDebuggeeHardwareAddress[6] = {0};
uint8_t KdpDebuggeeProtocolAddress[4] = {0};
uint16_t KdpDebuggeePort = 0;

uint8_t KdpDebuggerHardwareAddress[6] = {0};
uint8_t KdpDebuggerProtocolAddress[4] = {0};
uint16_t KdpDebuggerPort = 0;
bool KdpDebuggerConnected = false;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function dumps the stored UTF-16 error string into the screen.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void DumpErrorString(void) {
    if (!KdpDebugErrorString) {
        return;
    }

    /* This code works for "simple"/ASCII UTF-16, but it will break if any codepoint takes more than
     * a single uint16_t; It should be enough for the simple error messages from KDNET though. */
    size_t Size = 0;
    while (KdpDebugErrorString[Size]) {
        Size++;
    }

    char ErrorString[Size + 1];
    for (size_t i = 0; i < Size; i++) {
        ErrorString[i] = (char)KdpDebugErrorString[i];
    }

    ErrorString[Size] = 0;
    KdPrint(KD_TYPE_ERROR, "%s\n", ErrorString);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the kernel debugger (and synchronizes it with a host debugger) if
 *     requested in the configuration file.
 *
 * PARAMETERS:
 *     LoaderBlock - Data prepared by the boot loader for us.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KdpInitializeDebugger(KiLoaderBlock *LoaderBlock) {
    if (!LoaderBlock->Debug.Enabled) {
        KdPrint(KD_TYPE_INFO, "debugger disabled\n");
        return;
    }

    /* Environment initialization, required for all KDNET functions. */
    KdpDebugEnabled = true;
    KdpInitializeDeviceDescriptor(LoaderBlock);
    KdpInitializeExports();
    KdpInitializeImports();

    /* Attempt to initialize the extensibility module; They don't have any direct imports (via
     * OSLOADER), but they take in some simple kernel functions via the first parameter (and also
     * output some functions we can use via it). */
    uint32_t Status = ((KdpInitializeLibraryFn)LoaderBlock->Debug.Initializer)(
        &KdpDebugImports, NULL, &KdpDebugDevice);
    if (Status) {
        DumpErrorString();
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_DEBUGGER_INITIALIZATION_FAILURE,
            (uint64_t)Status,
            (uint64_t)KdpDebugErrorStatus,
            (uint64_t)KdpDebugHardwareId);
    }

    /* Start filling in the shared data structure. */
    memset(&KdpDebugData, 0, sizeof(KdpExtensibilityData));
    memset(&KdpDebuggeeHardwareAddress, 0, sizeof(KdpDebuggeeHardwareAddress));
    KdpDebugData.Device = &KdpDebugDevice;
    KdpDebugData.TargetMacAddress = KdpDebuggeeHardwareAddress;

    /* Is there even any driver that doesn't request memory? Anyways, we're too early to use the
     * normal allocator, so use the HAL early allocator. */
    uint32_t Size = KdpGetHardwareContextSize(&KdpDebugDevice);
    if (Size) {
        uint32_t Pages = (Size + MM_PAGE_SIZE - 1) >> MM_PAGE_SHIFT;
        uint64_t PhysicalAddress = MiAllocateEarlyPages(Pages);
        if (!PhysicalAddress) {
            KeFatalError(
                KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_DEBUGGER_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
                0,
                0);
        }

        void *VirtualAddress =
            HalpMapEarlyMemory(PhysicalAddress, Pages << MM_PAGE_SHIFT, MI_MAP_WRITE);
        if (!VirtualAddress) {
            KeFatalError(
                KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_DEBUGGER_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
                0,
                0);
        }

        KdpDebugDevice.Memory.Start = (KdpPhysicalAddress){.QuadPart = PhysicalAddress};
        KdpDebugDevice.Memory.VirtualAddress = VirtualAddress;
        KdpDebugDevice.Memory.Length = Size;
        KdpDebugDevice.Memory.Cached = true;
        KdpDebugDevice.Memory.Aligned = true;
        KdpDebugDevice.TransportData.HwContextSize = Size;
        KdpDebugDevice.Flags |= KDP_DEVICE_FLAGS_HAL_SCRATCH_ALLOCATED;
        KdpDebugData.Hardware = VirtualAddress;
        KdpDebugAdapter = VirtualAddress;
    }

    /* Attempt to bring up the network card; Main error we might find is if the host has a network
     * card of a supported vendor, but the device model itself is unsupported (and that's a panic
     * for us, disable debugging in the config file in this case). */
    KdPrint(KD_TYPE_DEBUG, "initializing the debug device controller\n");
    Status = KdpInitializeController(&KdpDebugData);
    if (Status) {
        DumpErrorString();
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_DEBUGGER_INITIALIZATION_FAILURE,
            (uint64_t)Status,
            (uint64_t)KdpDebugErrorStatus,
            (uint64_t)KdpDebugHardwareId);
    }

    /* Now our receive/send packet functions should be online. Wait until the remote debugger
     * connects to us. */
    memcpy(KdpDebuggeeProtocolAddress, LoaderBlock->Debug.Address, 4);
    KdpDebuggeePort = LoaderBlock->Debug.Port;

    KdPrint(
        KD_TYPE_INFO,
        "waiting for connection at %hhu.%hhu.%hhu.%hhu:%hu\n",
        KdpDebuggeeProtocolAddress[0],
        KdpDebuggeeProtocolAddress[1],
        KdpDebuggeeProtocolAddress[2],
        KdpDebuggeeProtocolAddress[3],
        KdpDebuggeePort);

    while (!KdpDebuggerConnected) {
        /* Wait for any incoming packet. */
        uint32_t Handle = 0;
        void *Packet = NULL;
        uint32_t Length = 0;
        uint32_t Status = KdpGetRxPacket(KdpDebugAdapter, &Handle, &Packet, &Length);
        if (Status) {
            continue;
        }

        /* Attempt to parse the ethernet frame (which hopefully contains either an ARP request for
         * us, or an UDP message requesting the debugger connection). */
        KdpParseEthernetFrame(Packet, Length);

        /* Return the resources back to the ethernet controller. */
        KdpReleaseRxPacket(KdpDebugAdapter, Handle);
    }

    KdPrint(
        KD_TYPE_INFO,
        "connected to %hhu.%hhu.%hhu.%hhu:%hu\n",
        KdpDebuggerProtocolAddress[0],
        KdpDebuggerProtocolAddress[1],
        KdpDebuggerProtocolAddress[2],
        KdpDebuggerProtocolAddress[3],
        KdpDebuggerPort);

    KdpDebugConnected = true;
}
