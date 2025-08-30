# SPDX-FileCopyrightText: (C) 2025 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

import socket
import struct

from . import interface
from . import protocol

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
            Packet = struct.pack(
                protocol.KDP_DEBUG_PACKET_FORMAT,
                protocol.KDP_DEBUG_PACKET_CONNECT_REQ)
            Socket.sendto(Packet, (DebuggeeProtocolAddress, DebuggeePort))

            # And wait for an ACK.
            Data, _ = Socket.recvfrom(2048)
            PacketType: int = struct.unpack(protocol.KDP_DEBUG_PACKET_FORMAT, Data[:1])[0]
            if PacketType == protocol.KDP_DEBUG_PACKET_CONNECT_ACK:
                return (True, protocol.KD_STATUS_SUCCESS)
        except socket.timeout:
            return (False, protocol.KD_STATUS_TIMEOUT)
        except KeyboardInterrupt:
            return (False, protocol.KD_STATUS_SUCCESS)
        except Exception as ExceptionData:
            interface.KdpShutdownInterface()
            print(f"error: {ExceptionData}")
            return (False, protocol.KD_STATUS_FAILURE)
