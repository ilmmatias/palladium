/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/kdp.h>
#include <os/intrin.h>
#include <rt/except.h>
#include <string.h>

extern void *KdpDebugAdapter;

extern uint8_t KdpDebuggeeHardwareAddress[6];
extern uint8_t KdpDebuggeeProtocolAddress[4];
extern uint16_t KdpDebuggeePort;

extern uint8_t KdpDebuggerHardwareAddress[6];
extern uint8_t KdpDebuggerProtocolAddress[4];
extern uint16_t KdpDebuggerPort;
extern bool KdpDebuggerConnected;

static char Buffer[1024] = {0};

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles any received debug packets during the early initialization stage.
 *
 * PARAMETERS:
 *     SourceHardwareAddress - MAC address of who sent us this packet.
 *     SourceProtocolAddress - IP(v4) address of who sent us this packet.
 *     SourcePort - UDP port of who sent us this packet.
 *     Packet - Header of the packet.
 *     Length - Size of the packet.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void ParseEarlyPacket(
    uint8_t SourceHardwareAddress[6],
    uint8_t SourceProtocolAddress[4],
    uint16_t SourcePort,
    KdpDebugPacket *Packet,
    uint32_t Length) {
    /* Only valid packet for this stage is the connection request, so ignore everything else. */
    (void)Length;
    if (Packet->Type != KDP_DEBUG_PACKET_CONNECT_REQ) {
        KdPrint(KD_TYPE_TRACE, "ignoring invalid debug packet of type %hhu\n", Packet->Type);
        return;
    }

    KdpDebugPacket AckPacket;
    AckPacket.Type = KDP_DEBUG_PACKET_CONNECT_ACK;

    uint32_t Status = KdpSendUdpPacket(
        SourceHardwareAddress,
        SourceProtocolAddress,
        KdpDebuggeePort,
        SourcePort,
        &AckPacket,
        sizeof(KdpDebugPacket));
    if (Status) {
        KdPrint(
            KD_TYPE_ERROR,
            "failed to ack the debugger connection request, error code = 0x%08x\n",
            Status);
    } else {
        KdpDebuggerConnected = true;
        memcpy(KdpDebuggerHardwareAddress, SourceHardwareAddress, 6);
        memcpy(KdpDebuggerProtocolAddress, SourceProtocolAddress, 4);
        KdpDebuggerPort = SourcePort;
        KdPrint(
            KD_TYPE_TRACE,
            "sent debugger connection ack to %hhu.%hhu.%hhu.%hhu:%hu\n",
            SourceProtocolAddress[0],
            SourceProtocolAddress[1],
            SourceProtocolAddress[2],
            SourceProtocolAddress[3],
            SourcePort);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles a received request to read some physical memory.
 *
 * PARAMETERS:
 *     Packet - Header of the packet.
 *     Length - Size of the packet.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void ParseReadPhysicalPacket(KdpDebugReadAddressPacket *Packet, uint32_t Length) {
    if (Length < sizeof(KdpDebugReadAddressPacket)) {
        KdPrint(KD_TYPE_TRACE, "ignoring invalid debug `rp` packet of size %u\n", Length);
        return;
    }

    /* We're in a very sensitive environment, so parameter validation is essential. */
    if (Packet->ItemSize != 1 && Packet->ItemSize != 2 && Packet->ItemSize != 4 &&
        Packet->ItemSize != 8) {
        KdPrint(
            KD_TYPE_TRACE,
            "ignoring invalid debug `rp` packet with item size %u\n",
            Packet->ItemSize);
        return;
    } else if (Packet->ItemCount * Packet->ItemSize != Packet->Length) {
        KdPrint(
            KD_TYPE_TRACE,
            "ignoring invalid debug `rp` packet with length %u (vs expected length of %u)\n",
            Packet->Length,
            Packet->ItemCount * Packet->ItemSize);
        return;
    } else if (Packet->Address + Packet->Length < Packet->Address) {
        KdPrint(
            KD_TYPE_TRACE,
            "ignoring invalid debug `rp` packet with address 0x%llx (as it overflows when "
            "reading %u bytes)\n",
            Packet->Address,
            Packet->Length);
        return;
    }

    KdpDebugReadAddressPacket *ResponsePacket = (KdpDebugReadAddressPacket *)Buffer;
    memcpy(ResponsePacket, Packet, sizeof(KdpDebugReadAddressPacket));
    ResponsePacket->Type = KDP_DEBUG_PACKET_READ_PHYSICAL_ACK;

    /* Don't bother with anything that overflows our response buffer. */
    if (Packet->Length > sizeof(Buffer) - sizeof(KdpDebugReadAddressPacket)) {
        ResponsePacket->ItemSize = 0;
        KdpSendUdpPacket(
            KdpDebuggerHardwareAddress,
            KdpDebuggerProtocolAddress,
            KdpDebuggeePort,
            KdpDebuggerPort,
            ResponsePacket,
            sizeof(KdpDebugReadAddressPacket));
        return;
    }

    /* And let's attempt to map using the early mapping engine (as it should be safe to use at any
     * time); Just a small problem, TODO: How do we check if this is a valid physical address?
     * Actually, what happens if we map and access a bad physical address, does it fault? Does it
     * just return non-sense (like all 0s or all Fs)? We really should handle this properly, as
     * we can't afford to fault again here. */
    void *VirtualAddress = HalpMapDebuggerMemory(Packet->Address, Packet->Length, 0);
    if (!VirtualAddress) {
        ResponsePacket->ItemSize = 0;
        KdpSendUdpPacket(
            KdpDebuggerHardwareAddress,
            KdpDebuggerProtocolAddress,
            KdpDebuggeePort,
            KdpDebuggerPort,
            ResponsePacket,
            sizeof(KdpDebugReadAddressPacket));
        return;
    }

    memcpy(ResponsePacket + 1, VirtualAddress, Packet->Length);
    HalpUnmapDebuggerMemory(VirtualAddress, Packet->Length);
    KdpSendUdpPacket(
        KdpDebuggerHardwareAddress,
        KdpDebuggerProtocolAddress,
        KdpDebuggeePort,
        KdpDebuggerPort,
        ResponsePacket,
        sizeof(KdpDebugReadAddressPacket) + Packet->Length);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles a received request to read some virtual memory.
 *
 * PARAMETERS:
 *     Packet - Header of the packet.
 *     Length - Size of the packet.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void ParseReadVirtualPacket(KdpDebugReadAddressPacket *Packet, uint32_t Length) {
    if (Length < sizeof(KdpDebugReadAddressPacket)) {
        KdPrint(KD_TYPE_TRACE, "ignoring invalid debug `rv` packet of size %u\n", Length);
        return;
    }

    /* We're in a very sensitive environment, so parameter validation is essential. */
    if (Packet->ItemSize != 1 && Packet->ItemSize != 2 && Packet->ItemSize != 4 &&
        Packet->ItemSize != 8) {
        KdPrint(
            KD_TYPE_TRACE,
            "ignoring invalid debug `rv` packet with item size %u\n",
            Packet->ItemSize);
        return;
    } else if (Packet->ItemCount * Packet->ItemSize != Packet->Length) {
        KdPrint(
            KD_TYPE_TRACE,
            "ignoring invalid debug `rv` packet with length %u (vs expected length of %u)\n",
            Packet->Length,
            Packet->ItemCount * Packet->ItemSize);
        return;
    } else if (Packet->Address + Packet->Length < Packet->Address) {
        KdPrint(
            KD_TYPE_TRACE,
            "ignoring invalid debug `rv` packet with address 0x%llx (as it overflows when "
            "reading %u bytes)\n",
            Packet->Address,
            Packet->Length);
        return;
    }

    KdpDebugReadAddressPacket *ResponsePacket = (KdpDebugReadAddressPacket *)Buffer;
    memcpy(ResponsePacket, Packet, sizeof(KdpDebugReadAddressPacket));
    ResponsePacket->Type = KDP_DEBUG_PACKET_READ_VIRTUAL_ACK;

    /* Don't bother with anything that overflows our response buffer. */
    if (Packet->Length > sizeof(Buffer) - sizeof(KdpDebugReadAddressPacket)) {
        ResponsePacket->ItemSize = 0;
        KdpSendUdpPacket(
            KdpDebuggerHardwareAddress,
            KdpDebuggerProtocolAddress,
            KdpDebuggeePort,
            KdpDebuggerPort,
            ResponsePacket,
            sizeof(KdpDebugReadAddressPacket));
        return;
    }

    /* We can't just GetPhysicalAddress+MapDebbugerMemory, as virtual memory might not be contigous.
     * Other than that, the logic is very similar to read physical. */
    char *Payload = (char *)(ResponsePacket + 1);
    uintptr_t CurrentAddress = Packet->Address;
    uint32_t RemainingLength = Packet->Length;
    while (RemainingLength) {
        uint32_t RegionLength = MM_PAGE_SIZE - (CurrentAddress & (MM_PAGE_SIZE - 1));
        if (RemainingLength < RegionLength) {
            RegionLength = RemainingLength;
        }

        uint64_t PhysicalAddress = HalpGetPhysicalAddress((void *)CurrentAddress);
        if (!PhysicalAddress) {
            ResponsePacket->ItemSize = 0;
            KdpSendUdpPacket(
                KdpDebuggerHardwareAddress,
                KdpDebuggerProtocolAddress,
                KdpDebuggeePort,
                KdpDebuggerPort,
                ResponsePacket,
                sizeof(KdpDebugReadAddressPacket));
            return;
        }

        void *VirtualAddress = HalpMapDebuggerMemory(PhysicalAddress, RegionLength, 0);
        if (!VirtualAddress) {
            ResponsePacket->ItemSize = 0;
            KdpSendUdpPacket(
                KdpDebuggerHardwareAddress,
                KdpDebuggerProtocolAddress,
                KdpDebuggeePort,
                KdpDebuggerPort,
                ResponsePacket,
                sizeof(KdpDebugReadAddressPacket));
            return;
        }

        memcpy(Payload, VirtualAddress, RegionLength);
        HalpUnmapDebuggerMemory(VirtualAddress, RegionLength);

        Payload += RegionLength;
        CurrentAddress += RegionLength;
        RemainingLength -= RegionLength;
    }

    KdpSendUdpPacket(
        KdpDebuggerHardwareAddress,
        KdpDebuggerProtocolAddress,
        KdpDebuggeePort,
        KdpDebuggerPort,
        ResponsePacket,
        sizeof(KdpDebugReadAddressPacket) + Packet->Length);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles a received request to read a system port.
 *
 * PARAMETERS:
 *     Packet - Header of the packet.
 *     Length - Size of the packet.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void ParseReadPortPacket(KdpDebugReadPortReqPacket *Packet, uint32_t Length) {
    if (Length < sizeof(KdpDebugReadPortReqPacket)) {
        KdPrint(KD_TYPE_TRACE, "ignoring invalid debug `rp` packet of size %u\n", Length);
        return;
    }

    /* Does it make sense to support something like ReadPortQWord? Does any architecture even
     * really implements this? */
    if (Packet->Size != 1 && Packet->Size != 2 && Packet->Size != 4) {
        KdPrint(
            KD_TYPE_TRACE, "ignoring invalid debug `rp` packet with item size %u\n", Packet->Size);
        return;
    }

    KdpDebugReadPortAckPacket *ResponsePacket = (KdpDebugReadPortAckPacket *)Buffer;
    memcpy(ResponsePacket, Packet, sizeof(KdpDebugReadPortReqPacket));
    ResponsePacket->Type = KDP_DEBUG_PACKET_READ_PORT_ACK;

    /* I'm not sure if we need __try here? We probably do for MMIO==PIO, but do we need it for
     * x86/true PIO? */
    __try {
        if (Packet->Size == 1) {
            ResponsePacket->Value = ReadPortByte(Packet->Address);
        } else if (Packet->Size == 2) {
            ResponsePacket->Value = ReadPortWord(Packet->Address);
        } else {
            ResponsePacket->Value = ReadPortDWord(Packet->Address);
        }
    } __except (RT_EXC_EXECUTE_HANDLER) {
        ResponsePacket->Size = 0;
    }

    KdpSendUdpPacket(
        KdpDebuggerHardwareAddress,
        KdpDebuggerProtocolAddress,
        KdpDebuggeePort,
        KdpDebuggerPort,
        ResponsePacket,
        sizeof(KdpDebugReadPortAckPacket));
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles any received debug packets after the early initialization stage
 *     (during break/panic).
 *
 * PARAMETERS:
 *     SourceHardwareAddress - MAC address of who sent us this packet.
 *     SourceProtocolAddress - IP(v4) address of who sent us this packet.
 *     SourcePort - UDP port of who sent us this packet.
 *     Packet - Header of the packet.
 *     Length - Size of the packet.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void ParseLatePacket(
    uint8_t SourceHardwareAddress[6],
    uint8_t SourceProtocolAddress[4],
    uint16_t SourcePort,
    KdpDebugPacket *Packet,
    uint32_t Length) {
    /* The debugger is already attached, so we need to validate if the message is coming from the
     * right client. */
    if (memcmp(SourceHardwareAddress, KdpDebuggerHardwareAddress, 6) != 0 ||
        memcmp(SourceProtocolAddress, KdpDebuggerProtocolAddress, 4) != 0 ||
        SourcePort != KdpDebuggerPort) {
        KdPrint(
            KD_TYPE_TRACE,
            "ignoring debug packet from unknown client %hhu.%hhu.%hhu.%hhu:%hu\n",
            SourceProtocolAddress[0],
            SourceProtocolAddress[1],
            SourceProtocolAddress[2],
            SourceProtocolAddress[3],
            SourcePort);
        return;
    }

    if (Packet->Type == KDP_DEBUG_PACKET_READ_PHYSICAL_REQ) {
        ParseReadPhysicalPacket((KdpDebugReadAddressPacket *)Packet, Length);
    } else if (Packet->Type == KDP_DEBUG_PACKET_READ_VIRTUAL_REQ) {
        ParseReadVirtualPacket((KdpDebugReadAddressPacket *)Packet, Length);
    } else if (Packet->Type == KDP_DEBUG_PACKET_READ_PORT_REQ) {
        ParseReadPortPacket((KdpDebugReadPortReqPacket *)Packet, Length);
    } else {
        KdPrint(KD_TYPE_TRACE, "ignoring invalid debug packet of type %u\n", Packet->Type);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles a received debug packet.
 *
 * PARAMETERS:
 *     State - Current execution state (initialization or break/panic).
 *     SourceHardwareAddress - MAC address of who sent us this packet.
 *     SourceProtocolAddress - IP(v4) address of who sent us this packet.
 *     SourcePort - UDP port of who sent us this packet.
 *     Packet - Header of the packet.
 *     Length - Size of the packet.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KdpParseDebugPacket(
    int State,
    uint8_t SourceHardwareAddress[6],
    uint8_t SourceProtocolAddress[4],
    uint16_t SourcePort,
    KdpDebugPacket *Packet,
    uint32_t Length) {
    if (Length < sizeof(KdpDebugPacket)) {
        KdPrint(KD_TYPE_TRACE, "ignoring invalid debug packet of size %u\n", Length);
        return;
    } else if (State == KDP_STATE_EARLY) {
        ParseEarlyPacket(SourceHardwareAddress, SourceProtocolAddress, SourcePort, Packet, Length);
    } else {
        ParseLatePacket(SourceHardwareAddress, SourceProtocolAddress, SourcePort, Packet, Length);
    }
}
