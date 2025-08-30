# SPDX-FileCopyrightText: (C) 2025 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

import socket
import struct

from . import interface
from . import protocol
from . import utils

#--------------------------------------------------------------------------------------------------
# PURPOSE:
#     This function handles the received `rp` or `rv` data from the kernel.
#
# PARAMETERS:
#     PacketType - Which packet this is.
#     Data - What we got back.
#
# RETURN VALUE:
#     None.
#--------------------------------------------------------------------------------------------------
def KdpHandleReadMemoryAck(PacketType: int, Data: bytes) -> None:
    if PacketType == protocol.KDP_DEBUG_PACKET_READ_PHYSICAL_ACK:
        ExpectedState = protocol.KDP_STATE_READ_PHYSICAL
        PacketCommand = "rp"
        PacketFormat = protocol.KDP_DEBUG_PACKET_READ_PHYSICAL_FORMAT
        MemoryType = "physical"
    else:
        ExpectedState = protocol.KDP_STATE_READ_VIRTUAL
        PacketCommand = "rv"
        PacketFormat = protocol.KDP_DEBUG_PACKET_READ_VIRTUAL_FORMAT
        MemoryType = "virtual"

    # Should we be indeed printing something to the command window in this case?
    if protocol.KdpCurrentState != ExpectedState:
        interface.KdPrint(
            interface.KD_DEST_COMMAND,
            interface.KD_TYPE_ERROR,
            f"received unexpected `{PacketCommand}` acknowledgement\n")
        return

    protocol.KdpCurrentState = protocol.KDP_STATE_NONE

    if len(Data) < 18:
        interface.KdPrint(
            interface.KD_DEST_COMMAND,
            interface.KD_TYPE_ERROR,
            f"received corrupted `{PacketCommand}` acknowledgement\n")
        return

    IncomingStruct = struct.unpack(PacketFormat, Data[:18])
    Address: int = IncomingStruct[1]
    ItemSize: int = IncomingStruct[2]
    ItemCount: int = IncomingStruct[3]
    Length: int = IncomingStruct[4]

    # Early short-circuit if the kernel failed to map the data.
    if not ItemSize:
        interface.KdPrint(
            interface.KD_DEST_COMMAND,
            interface.KD_TYPE_ERROR,
            f"failed to read the specified {MemoryType} memory range\n")
        return

    # Parameter validation (just to ensure no one can break us by manually sending bad packets).
    if ItemSize not in (1, 2, 4, 8) or \
       ItemCount * ItemSize != Length or \
       len(Data) - 18 < Length:
        interface.KdPrint(
            interface.KD_DEST_COMMAND,
            interface.KD_TYPE_ERROR,
            f"received corrupted `{PacketCommand}` acknowledgement\n")
        return

    utils.KdpPrintMemoryData(MemoryType, Data[18:], Address, ItemSize, Length)

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
        PacketType: int = struct.unpack(protocol.KDP_DEBUG_PACKET_FORMAT, Data[:1])[0]
        if PacketType == protocol.KDP_DEBUG_PACKET_PRINT:
            Message = Data[1:].decode("utf-8")
            interface.KdPrint(interface.KD_DEST_KERNEL, interface.KD_TYPE_NONE, Message)
        elif PacketType == protocol.KDP_DEBUG_PACKET_BREAK:
            AllowInput = True
        elif PacketType == protocol.KDP_DEBUG_PACKET_READ_PHYSICAL_ACK or \
             PacketType == protocol.KDP_DEBUG_PACKET_READ_VIRTUAL_ACK:
            KdpHandleReadMemoryAck(PacketType, Data)
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
