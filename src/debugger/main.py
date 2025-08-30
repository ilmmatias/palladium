# SPDX-FileCopyrightText: (C) 2025 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

import argparse
import ipaddress
import socket

from . import interface
from . import protocol

# Definitions related to our custom debugger protocol.
KDP_DEBUG_PACKET_CONNECT_REQ = 0
KDP_DEBUG_PACKET_CONNECT_ACK = 1
KDP_DEBUG_PACKET_PRINT = 2
KDP_DEBUG_PACKET_BREAK = 3

# Format for the custom debugger protocol structure.
KDP_DEBUG_PACKET_FORMAT = "<B"

#--------------------------------------------------------------------------------------------------
# PURPOSE:
#     This function serves as the entry point for the debugger.
#
# PARAMETERS:
#     None.
#
# RETURN VALUE:
#     Integer, usually 0 or 1 depending on success (0) or failure (!=0).
#--------------------------------------------------------------------------------------------------
def main() -> int:
    # Start by grabbing the debuggee's IP+port from the command line.
    ArgumentParser = argparse.ArgumentParser(description="Kernel debugger client for Palladium")
    ArgumentParser.add_argument("ip", type=str, help="IP(v4) address of the debuggee")
    ArgumentParser.add_argument("port", type=int, help="Port of the debuggee")

    # Just make sure to validate the values (the port is always an unsigned 16-bit value,
    # and we should be able to validate the IP using the builtin module)
    Arguments = ArgumentParser.parse_args()
    DebuggeeProtocolAddress: str = Arguments.ip
    DebuggeePort: int = Arguments.port

    if DebuggeePort < 0 or DebuggeePort > 65535:
        print(f"error: invalid port number: {DebuggeePort}")
        return 1

    if DebuggeeProtocolAddress != "localhost":
        try:
            ipaddress.IPv4Address(DebuggeeProtocolAddress)
        except ValueError:
            print(f"error: invalid IP address: {DebuggeeProtocolAddress}")
            return 1

    # Create the UDP socket and attempt doing a handshake with the debuggee.
    try:
        Socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        Socket.settimeout(0.01)
    except Exception as ExceptionData:
        print(f"error: failed to create the UDP socket: {ExceptionData}")
        return 1

    # Get the TUI online before anything else.
    interface.KdpInitializeInterface()

    while True:
        # One pass of input handling (input not allowed yet, but we do handle
        # scroll-related events and resizing).
        _ = interface.KdReadInput(False, False, "", "")

        # And one pass of network related handling.
        (Connected, Status) = protocol.KdEstablishConnection(
            Socket,
            DebuggeeProtocolAddress,
            DebuggeePort)
        if Connected:
            break
        elif Status != protocol.KD_STATUS_TIMEOUT:
            if Status == 0:
                interface.KdpShutdownInterface()
            Socket.close()
            return Status

    interface.KdChangeInputMessage("target system is running...")

    # TODO: We should probably add a periodic ping to check if the connection is still active?
    PromptState = False
    AllowInput = False
    InputLine = ""
    while True:
        # One pass of input handling (for scroll when just reading the kernel output,
        # or for the input line too when the kernel reaches a breakpoint).
        (PromptState, InputLine, InputFinished) = \
            interface.KdReadInput(PromptState, AllowInput, InputLine, "kd> ")
        if InputFinished:
            protocol.KdHandleInput(Socket, DebuggeeProtocolAddress, DebuggeePort, InputLine)
            InputLine = ""

        # And one pass of network related handling.
        (FinishExecution, EncounteredBreak, Status) = protocol.KdHandleIncomingPacket(Socket)
        if FinishExecution:
            if Status == 0:
                interface.KdpShutdownInterface()
            Socket.close()
            return Status
        elif EncounteredBreak:
            AllowInput = True
