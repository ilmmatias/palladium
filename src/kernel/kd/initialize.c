/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/kd.h>
#include <kernel/kdp.h>
#include <kernel/ke.h>
#include <kernel/ki.h>
#include <kernel/mi.h>
#include <kernel/mm.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern KdpDebugDeviceDescriptor KdpDebugDevice;
extern KdpExtensibilityImports KdpDebugImports;
extern uint32_t KdpDebugErrorStatus;
extern uint16_t *KdpDebugErrorString;
extern uint32_t KdpDebugHardwareId;

bool KdpDebugEnabled = false;
bool KdpDebugEchoEnabled = true;
bool KdpDebugConnected = false;
uint32_t KdpDebugTransport = KI_LOADER_DEBUG_TRANSPORT_NONE;
uint32_t KdpDebugDisconnectPolicy = KI_LOADER_DEBUG_DISCONNECT_STOP;
uint32_t KdpDebugDisconnectTimeoutMilliseconds = 5000;
void *KdpDebugAdapter = NULL;
KdpExtensibilityData KdpDebugData = {};

uint8_t KdpDebuggeeHardwareAddress[6] = {0};
uint8_t KdpDebuggeeProtocolAddress[4] = {0};
uint16_t KdpDebuggeePort = 0;

uint8_t KdpDebuggerHardwareAddress[6] = {0};
uint8_t KdpDebuggerProtocolAddress[4] = {0};
uint16_t KdpDebuggerPort = 0;
static KeWork PollWork;
static bool RuntimePollingEnabled;
static bool RuntimePollingAnnounced;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function performs one bounded debugger transport poll at dispatch level.
 *
 * PARAMETERS:
 *     Context - Unused work context.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void PollRoutine(void *) {
    if (!RuntimePollingAnnounced) {
        RuntimePollingAnnounced = true;
        KdPrint(KD_TYPE_INFO, "runtime debugger polling enabled\n");
    }
    KdpPollTransport(KDP_STATE_LATE);
    KdpCheckHeartbeat();
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function queues the single static runtime debugger poll operation.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KdpQueuePoll(void) {
    if (RuntimePollingEnabled && KeGetCurrentProcessor()->Number == 0) {
        KeQueueWork(&PollWork, true);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function polls whichever debugger transport was selected by the loader.
 *
 * PARAMETERS:
 *     State - Early handshake or runtime debugger state.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KdpPollTransport(int State) {
    if (KdpDebugTransport == KI_LOADER_DEBUG_TRANSPORT_PC16550_PIO) {
        KdpPollSerialTransport(State);
        return;
    }

    if (KdpDebugTransport != KI_LOADER_DEBUG_TRANSPORT_KDNET) {
        return;
    }

    uint32_t Handle = 0;
    void *Packet = NULL;
    uint32_t Length = 0;
    if (!KdpGetRxPacket(KdpDebugAdapter, &Handle, &Packet, &Length)) {
        KdpParseEthernetFrame(State, Packet, Length);
        KdpReleaseRxPacket(KdpDebugAdapter, Handle);
    }
}

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
 *     This function enters a loop waiting for any messages coming from the debug ethernet device.
 *
 * PARAMETERS:
 *     State - Current execution state (initialization or break/panic).
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KdpEnterReceiveLoop(int State) {
    /* Should KdpInitializeDebugger do this, so is it proper for us to do it? */
    if (State == KDP_STATE_EARLY) {
        KdPrint(
            KD_TYPE_INFO,
            "waiting for connection at %hhu.%hhu.%hhu.%hhu:%hu\n",
            KdpDebuggeeProtocolAddress[0],
            KdpDebuggeeProtocolAddress[1],
            KdpDebuggeeProtocolAddress[2],
            KdpDebuggeeProtocolAddress[3],
            KdpDebuggeePort);
    }

    while (true) {
        /* Early initialization has a condition to exit this loop (KdpDebuggerConnected switching to
         * true), but late/break receive loops just run forever (at least until we allow unbreaking
         * the execution). */
        if (State == KDP_STATE_EARLY && KdpDebugConnected) {
            break;
        }

        if (State == KDP_STATE_LATE && KdpGetTargetState() == KDP_TARGET_RUNNING) {
            break;
        }

        KdpPollTransport(State);
        PauseProcessor();
    }

    if (State == KDP_STATE_EARLY) {
        KdPrint(
            KD_TYPE_INFO,
            "connected to %hhu.%hhu.%hhu.%hhu:%hu\n",
            KdpDebuggerProtocolAddress[0],
            KdpDebuggerProtocolAddress[1],
            KdpDebuggerProtocolAddress[2],
            KdpDebuggerProtocolAddress[3],
            KdpDebuggerPort);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the kernel debugger (and synchronizes it with a host debugger)
 *if requested in the configuration file.
 *
 * PARAMETERS:
 *     LoaderBlock - Data prepared by the boot loader for us.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KdpInitializeDebugger(KiLoaderBlock *LoaderBlock) {
    if (LoaderBlock->Debug.Type == KI_LOADER_DEBUG_TRANSPORT_NONE) {
        KdPrint(KD_TYPE_INFO, "debugger disabled\n");
        return;
    }

    KdpDebugEnabled = true;
    KdpDebugTransport = LoaderBlock->Debug.Type;
    KdpDebugDisconnectPolicy = LoaderBlock->Debug.DisconnectPolicy;
    KdpDebugDisconnectTimeoutMilliseconds = LoaderBlock->Debug.DisconnectTimeoutMilliseconds;

    if (KdpDebugTransport == KI_LOADER_DEBUG_TRANSPORT_PC16550_PIO) {
        if (!KdpInitializeSerialTransport(LoaderBlock)) {
            KeFatalError(
                KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
                KE_PANIC_PARAMETER_DEBUGGER_INITIALIZATION_FAILURE,
                0,
                0,
                0);
        }
    } else if (KdpDebugTransport == KI_LOADER_DEBUG_TRANSPORT_KDNET) {
        /* Environment initialization required by the legally supplied KDNET module. */
        KdpInitializeDeviceDescriptor(LoaderBlock);
        KdpInitializeExports();
        KdpInitializeImports();

        uint32_t Status = ((KdpInitializeLibraryFn)LoaderBlock->Debug.KdNet.Initializer)(
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

        /* Attempt to bring up the network card; Main error we might find is if the host has a
         * network card of a supported vendor, but the device model itself is unsupported (and
         * that's a panic for us, disable debugging in the config file in this case). */
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

        memcpy(KdpDebuggeeProtocolAddress, LoaderBlock->Debug.KdNet.Address, 4);
        KdpDebuggeePort = LoaderBlock->Debug.KdNet.Port;
    } else {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_DEBUGGER_INITIALIZATION_FAILURE,
            0,
            0,
            0);
    }

    KdpEnterReceiveLoop(KDP_STATE_EARLY);

    /* From now on, we respect the echo enabled settings (as the debugger is online, and can receive
     * the messages even if we don't print them out in the display). */
    if (!(LoaderBlock->Debug.Flags & KI_LOADER_DEBUG_FLAG_ECHO_ENABLED)) {
        KdPrint(KD_TYPE_INFO, "debug echoing disabled\n");
        KdpDebugEchoEnabled = false;
    }

    KdpSetTargetRunning();
    KeInitializeWork(&PollWork, PollRoutine, NULL);
    RuntimePollingEnabled = true;
}
