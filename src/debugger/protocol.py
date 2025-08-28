# SPDX-FileCopyrightText: (C) 2025 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

import socket
import struct

from . import interface

# Definitions related to our custom debugger protocol.
KDP_DEBUG_PACKET_CONNECT_REQ = 0x00
KDP_DEBUG_PACKET_PRINT = 0x01
KDP_DEBUG_PACKET_BREAK = 0x02
KDP_DEBUG_PACKET_READ_PHYSICAL_REQ = 0x03
KDP_DEBUG_PACKET_READ_VIRTUAL_REQ = 0x04
KDP_DEBUG_PACKET_READ_PORT_REQ = 0x05
KDP_DEBUG_PACKET_READ_REGISTERS_REQ = 0x06

# ACKs always have the higher (7th) bit set.
KDP_DEBUG_PACKET_CONNECT_ACK = 0x80
KDP_DEBUG_PACKET_READ_PHYSICAL_ACK = 0x83
KDP_DEBUG_PACKET_READ_VIRTUAL_ACK = 0x84
KDP_DEBUG_PACKET_READ_PORT_ACK = 0x85
KDP_DEBUG_PACKET_READ_REGISTERS_ACK = 0x86

# Format for the custom debugger protocol structure.
KDP_DEBUG_PACKET_FORMAT = "<B"
KDP_DEBUG_PACKET_READ_PHYSICAL_FORMAT = "<BQBLL"
KDP_DEBUG_PACKET_READ_VIRTUAL_FORMAT = "<BQBLL"

# Definitions related to the current state/context.
KDP_STATE_NONE = 0
KDP_STATE_READ_PHYSICAL = 1
KDP_STATE_READ_VIRTUAL = 1

# Internal context.
KdpCurrentState = KDP_STATE_NONE

#--------------------------------------------------------------------------------------------------
# PURPOSE:
#     This function attempts to establish a connection to the debuggee using the given UDP socket.
#
# PARAMETERS:
#     Socket - What socket we're using.
#     DebuggeeProtocolAddress - IP(v4) address of the debuggee.
#     DebuggeePort - Target UDP port of the debuggee.
#
# RETURN VALUE:
#     A tuple, containing true/false depending on if we successfully connected to the debuggee, and
#     an integer afterwards with what main() should return if we didn't connect.
#--------------------------------------------------------------------------------------------------
def KdEstablishConnection(
    Socket: socket.socket,
    DebuggeeProtocolAddress: str,
    DebuggeePort: int) -> tuple[bool, int]:
    while True:
        try:
            # Send a REQ.
            Packet = struct.pack(KDP_DEBUG_PACKET_FORMAT, KDP_DEBUG_PACKET_CONNECT_REQ)
            Socket.sendto(Packet, (DebuggeeProtocolAddress, DebuggeePort))

            # And wait for an ACK.
            Data, _ = Socket.recvfrom(2048)
            PacketType: int = struct.unpack(KDP_DEBUG_PACKET_FORMAT, Data[:1])[0]
            if PacketType == KDP_DEBUG_PACKET_CONNECT_ACK:
                return (True, 0)
        except socket.timeout:
            pass
        except KeyboardInterrupt:
            return (False, 0)
        except Exception as ExceptionData:
            interface.KdpShutdownInterface()
            print(f"error: {ExceptionData}")
            return (False, 1)

#--------------------------------------------------------------------------------------------------
# PURPOSE:
#     This function handles the printing out any received memory data from the kernel.
#
# PARAMETERS:
#     Kind - Which memory type we're reading (physical vs virtual).
#     Payload - What memory data we should print.
#     Address - What address we read.
#     ItemSize - Size of each item to print.
#     Length - Total size in bytes of the region we're dumping.
#
# RETURN VALUE:
#     None.
#--------------------------------------------------------------------------------------------------
def KdpPrintMemoryData(
    Kind: str,
    Payload: bytes,
    Address: int,
    ItemSize: int,
    Length: int) -> None:
    # Map related to how we should parse the buffer items.
    FormatMap = {
        1: ("<B", "02x", 16),
        2: ("<H", "04x", 8),
        4: ("<L", "08x", 4),
        8: ("<Q", "016x", 2)
    }

    UnpackFormat, ItemFormat, ItemsPerLine = FormatMap[ItemSize]
    BytesPerLine = ItemsPerLine * ItemSize

    interface.KdPrint(
        interface.KD_DEST_COMMAND,
        interface.KD_TYPE_INFO,
        f"showing data contained in the {Kind} range " +
        f"0x{Address:x} - 0x{(Address + Length):x}\n")

    for i in range(0, len(Payload), BytesPerLine):
        # Unpack all items inside the current chunk (so we can later join them).
        Chunk = Payload[i:i + BytesPerLine]
        Items = []
        for j in range(0, len(Chunk), ItemSize):
            ItemBytes = Chunk[j:j + ItemSize]
            if len(ItemBytes) < ItemSize:
                break
            ItemValue: int = struct.unpack(UnpackFormat, ItemBytes)[0]
            Items.append(f"{ItemValue:{ItemFormat}}")

        AddressString = f"{(Address + i):016x}: "
        HexString = " ".join(Items)

        # And also show their ASCII representation (only signle-byte mode though).
        if ItemSize == 1:
            AsciiString = "".join(chr(Item) if 32 <= Item < 127 else "." for Item in Chunk)
            AsciiString = f" |{AsciiString}|\n"
        else:
            AsciiString = "\n"

        # And show everything + the current address.
        interface.KdPrint(
            interface.KD_DEST_COMMAND,
            interface.KD_TYPE_NONE,
            AddressString + HexString + AsciiString)

#--------------------------------------------------------------------------------------------------
# PURPOSE:
#     This function handles the received `rp` (read physical) data from the kernel.
#
# PARAMETERS:
#     Data - What we got back.
#
# RETURN VALUE:
#     None.
#--------------------------------------------------------------------------------------------------
def KdpHandleReadPhysicalAck(Data: bytes) -> None:
    global KdpCurrentState

    # Should we be indeed printing something to the command window in this case?
    if KdpCurrentState != KDP_STATE_READ_PHYSICAL:
        interface.KdPrint(
            interface.KD_DEST_COMMAND,
            interface.KD_TYPE_ERROR,
            "received unexpected `rp` acknowledgement\n")
        return

    KdpCurrentState = KDP_STATE_NONE

    if len(Data) < 18:
        interface.KdPrint(
            interface.KD_DEST_COMMAND,
            interface.KD_TYPE_ERROR,
            "received corrupted `rp` acknowledgement\n")
        return

    IncomingStruct = struct.unpack(KDP_DEBUG_PACKET_READ_PHYSICAL_FORMAT, Data[:18])
    Address: int = IncomingStruct[1]
    ItemSize: int = IncomingStruct[2]
    ItemCount: int = IncomingStruct[3]
    Length: int = IncomingStruct[4]

    # Early short-circuit if the kernel failed to map the data.
    if not ItemSize:
        interface.KdPrint(
            interface.KD_DEST_COMMAND,
            interface.KD_TYPE_ERROR,
            "failed to read the specified physical memory range\n")
        return

    # Parameter validation (just to ensure no one can break us by manually sending bad packets).
    if ItemSize not in (1, 2, 4, 8) or \
       ItemCount * ItemSize != Length or \
       len(Data) - 18 < Length:
        interface.KdPrint(
            interface.KD_DEST_COMMAND,
            interface.KD_TYPE_ERROR,
            "received corrupted `rp` acknowledgement\n")
        return

    KdpPrintMemoryData("physical", Data[18:], Address, ItemSize, Length)

#--------------------------------------------------------------------------------------------------
# PURPOSE:
#     This function handles the received `rv` (read virtual) data from the kernel.
#
# PARAMETERS:
#     Data - What we got back.
#
# RETURN VALUE:
#     None.
#--------------------------------------------------------------------------------------------------
def KdpHandleReadVirtualAck(Data: bytes) -> None:
    global KdpCurrentState

    # Should we be indeed printing something to the command window in this case?
    if KdpCurrentState != KDP_STATE_READ_VIRTUAL:
        interface.KdPrint(
            interface.KD_DEST_COMMAND,
            interface.KD_TYPE_ERROR,
            "received unexpected `rv` acknowledgement\n")
        return

    KdpCurrentState = KDP_STATE_NONE

    if len(Data) < 18:
        interface.KdPrint(
            interface.KD_DEST_COMMAND,
            interface.KD_TYPE_ERROR,
            "received corrupted `rv` acknowledgement\n")
        return

    IncomingStruct = struct.unpack(KDP_DEBUG_PACKET_READ_VIRTUAL_FORMAT, Data[:18])
    Address: int = IncomingStruct[1]
    ItemSize: int = IncomingStruct[2]
    ItemCount: int = IncomingStruct[3]
    Length: int = IncomingStruct[4]

    # Early short-circuit if the kernel failed to map the data.
    if not ItemSize:
        interface.KdPrint(
            interface.KD_DEST_COMMAND,
            interface.KD_TYPE_ERROR,
            "failed to read the specified virtual memory range\n")
        return

    # Parameter validation (just to ensure no one can break us by manually sending bad packets).
    if ItemSize not in (1, 2, 4, 8) or \
       ItemCount * ItemSize != Length or \
       len(Data) - 18 < Length:
        interface.KdPrint(
            interface.KD_DEST_COMMAND,
            interface.KD_TYPE_ERROR,
            "received corrupted `rv` acknowledgement\n")
        return

    KdpPrintMemoryData("virtual", Data[18:], Address, ItemSize, Length)

#--------------------------------------------------------------------------------------------------
# PURPOSE:
#     This function handles parsing an incoming debug packet.
#
# PARAMETERS:
#     Socket - What socket we're using.
#
# RETURN VALUE:
#     A tuple, first containing true/false depending on if we should just finish execution, then
#     true/false depending on if we encountered the BREAK packet, and finally an integer
#     representing what main() should return if we should exit. 
#--------------------------------------------------------------------------------------------------
def KdHandleIncomingPacket(Socket: socket.socket) -> tuple[bool, bool, int]:
    AllowInput = False

    try:
        Data, _ = Socket.recvfrom(2048)
        PacketType: int = struct.unpack(KDP_DEBUG_PACKET_FORMAT, Data[:1])[0]
        if PacketType == KDP_DEBUG_PACKET_PRINT:
            Message = Data[1:].decode("utf-8")
            interface.KdPrint(interface.KD_DEST_KERNEL, interface.KD_TYPE_NONE, Message)
        elif PacketType == KDP_DEBUG_PACKET_BREAK:
            AllowInput = True
        elif PacketType == KDP_DEBUG_PACKET_READ_PHYSICAL_ACK:
            KdpHandleReadPhysicalAck(Data)
        elif PacketType == KDP_DEBUG_PACKET_READ_VIRTUAL_ACK:
            KdpHandleReadVirtualAck(Data)
        else:
            interface.KdPrint(
                interface.KD_DEST_COMMAND,
                interface.KD_TYPE_TRACE,
                f"ignoring invalid debug packet of type {PacketType}")
    except socket.timeout:
        pass
    except KeyboardInterrupt:
        return (True, False, 0)
    except Exception as ExceptionData:
        interface.KdpShutdownInterface()
        print(f"error: {ExceptionData}")
        return (True, False, 1)

    return (False, AllowInput, 0)

#--------------------------------------------------------------------------------------------------
# PURPOSE:
#     This function handles sending a `rp` (read physical) request to the kernel.
#
# PARAMETERS:
#     Socket - What socket we're using.
#     DebuggeeProtocolAddress - IP(v4) address of the debuggee.
#     DebuggeePort - Target UDP port of the debuggee.
#     InputTokens - What we read from the user.
#
# RETURN VALUE:
#     None.
#--------------------------------------------------------------------------------------------------
def KdpHandleReadPhysicalRequest(
    Socket: socket.socket,
    DebuggeeProtocolAddress: str,
    DebuggeePort: int,
    InputTokens: list[str]) -> None:
    global KdpCurrentState

    try:
        # rp/AB C
        #     A -> b(yte) / w(ord) / d(word) / q(word).
        #     B -> Unsigned number; How many bytes/words/dwords/qwords to try reading.
        #     C -> Address.
        if len(InputTokens) != 2 or \
           len(InputTokens[0]) < 4 or \
           not InputTokens[0].startswith("rp/"):
            raise ValueError("expected format: rp/<size>[count] <address>")

        ItemSize = InputTokens[0][3]
        SizeMap = {"b": 1, "w": 2, "d": 4, "q": 8}
        if ItemSize not in SizeMap:
            raise ValueError("expected format: rp/<size>[count] <address>")
        ItemSize = SizeMap[ItemSize]

        ItemCount = 128 // ItemSize
        if len(InputTokens[0]) > 4:
            ItemCount = int(InputTokens[0][4:])

        Length = ItemCount * ItemSize
        Address = int(InputTokens[1], 16)
    except ValueError as ExceptionData:
        interface.KdPrint(
            interface.KD_DEST_COMMAND,
            interface.KD_TYPE_ERROR,
            f"{ExceptionData}\n")
        return

    Packet = struct.pack(
        KDP_DEBUG_PACKET_READ_PHYSICAL_FORMAT,
        KDP_DEBUG_PACKET_READ_PHYSICAL_REQ,
        Address,
        ItemSize,
        ItemCount,
        Length)
    Socket.sendto(Packet, (DebuggeeProtocolAddress, DebuggeePort))
    KdpCurrentState = KDP_STATE_READ_PHYSICAL

#--------------------------------------------------------------------------------------------------
# PURPOSE:
#     This function handles sending a `rv` (read virtual) request to the kernel.
#
# PARAMETERS:
#     Socket - What socket we're using.
#     DebuggeeProtocolAddress - IP(v4) address of the debuggee.
#     DebuggeePort - Target UDP port of the debuggee.
#     InputTokens - What we read from the user.
#
# RETURN VALUE:
#     None.
#--------------------------------------------------------------------------------------------------
def KdpHandleReadVirtualRequest(
    Socket: socket.socket,
    DebuggeeProtocolAddress: str,
    DebuggeePort: int,
    InputTokens: list[str]) -> None:
    global KdpCurrentState

    try:
        # rv/AB C
        #     A -> b(yte) / w(ord) / d(word) / q(word).
        #     B -> Unsigned number; How many bytes/words/dwords/qwords to try reading.
        #     C -> Address.
        if len(InputTokens) != 2 or \
           len(InputTokens[0]) < 4 or \
           not InputTokens[0].startswith("rv/"):
            raise ValueError("expected format: rv/<size>[count] <address>")

        ItemSize = InputTokens[0][3]
        SizeMap = {"b": 1, "w": 2, "d": 4, "q": 8}
        if ItemSize not in SizeMap:
            raise ValueError("expected format: rv/<size>[count] <address>")
        ItemSize = SizeMap[ItemSize]

        ItemCount = 128 // ItemSize
        if len(InputTokens[0]) > 4:
            ItemCount = int(InputTokens[0][4:])

        Length = ItemCount * ItemSize
        Address = int(InputTokens[1], 16)
    except ValueError as ExceptionData:
        interface.KdPrint(
            interface.KD_DEST_COMMAND,
            interface.KD_TYPE_ERROR,
            f"{ExceptionData}\n")
        return

    Packet = struct.pack(
        KDP_DEBUG_PACKET_READ_VIRTUAL_FORMAT,
        KDP_DEBUG_PACKET_READ_VIRTUAL_REQ,
        Address,
        ItemSize,
        ItemCount,
        Length)
    Socket.sendto(Packet, (DebuggeeProtocolAddress, DebuggeePort))
    KdpCurrentState = KDP_STATE_READ_VIRTUAL

#--------------------------------------------------------------------------------------------------
# PURPOSE:
#     This function handles a newly completed user input line, sending any required debug packets.
#
# PARAMETERS:
#     Socket - What socket we're using.
#     DebuggeeProtocolAddress - IP(v4) address of the debuggee.
#     DebuggeePort - Target UDP port of the debuggee.
#     InputLine - What we read from the user.
#
# RETURN VALUE:
#     None.
#--------------------------------------------------------------------------------------------------
def KdHandleInput(
    Socket: socket.socket,
    DebuggeeProtocolAddress: str,
    DebuggeePort: int,
    InputLine: str) -> None:
    InputTokens = InputLine.split()

    if InputTokens[0] == "rp" or InputTokens[0].startswith("rp/"):
        KdpHandleReadPhysicalRequest(Socket, DebuggeeProtocolAddress, DebuggeePort, InputTokens)
    elif InputTokens[0] == "rv" or InputTokens[0].startswith("rv/"):
        KdpHandleReadVirtualRequest(Socket, DebuggeeProtocolAddress, DebuggeePort, InputTokens)
    else:
        interface.KdPrint(
            interface.KD_DEST_COMMAND,
            interface.KD_TYPE_ERROR,
            f"invalid command `{InputLine}`\n")
