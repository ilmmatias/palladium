# SPDX-FileCopyrightText: (C) 2025 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

import socket
import struct

from . import interface
from . import protocol

#--------------------------------------------------------------------------------------------------
# PURPOSE:
#     This function handles an `el` (export log) request.
#
# PARAMETERS:
#     InputTokens - What we read from the user.
#
# RETURN VALUE:
#     None.
#--------------------------------------------------------------------------------------------------
def KdpHandleExportLogRequest(InputTokens: list[str]) -> None:
    # el/A B
    #     A -> k(ernel) / c(ommand).
    #     B -> Path to save the buffer.

    if len(InputTokens) != 2 or \
       (InputTokens[0].startswith("el/") and len(InputTokens[0]) != 4):
        raise ValueError("expected format: el[/target] <path>")

    if InputTokens[0].startswith("el/"):
        TargetMap = {"k": interface.KD_DEST_KERNEL, "c": interface.KD_DEST_COMMAND}
        if InputTokens[0][3] not in TargetMap:
            raise ValueError("expected format: el[/target] <path>")
        Target = TargetMap[InputTokens[0][3]]
    else:
        Target = interface.KdpSelectedOutput

    Path = InputTokens[1]
    interface.KdExportBuffer(Target, Path)

#--------------------------------------------------------------------------------------------------
# PURPOSE:
#     This function handles a help request.
#
# PARAMETERS:
#     None.
#
# RETURN VALUE:
#     None.
#--------------------------------------------------------------------------------------------------
def KdpHandleHelpRequest() -> None:
    HelpText = """
keyboard controls:
    tab                        - swap focus between the kernel and output logs
    up/down                    - scroll the focused log one line up/down
    page up/down               - scroll the focused log one page up/down
    ctrl+c                     - closes this application

mouse controls:
    scroll up/down             - scroll the focused log up/down

commands:
    el[/target] <path>         - save the specified log to a file
                                 by default, the focused log will be saved
                                 [target] can be either `k` (kernel) or `c` (command)
    h                          - show this help dialog
    help                       - alias to `h`
    q                          - closes this application
    quit                       - alias to `q`
    rp/<size>[count] <address> - tries to read some data at the specified physical address
                                 by default, 128 bytes of data will be read
                                 <size> can be `b` (8-bits), `w` (16-bits), `d` (32-bits),
                                 or `q` (64-bits)
                                 [count] is how many elements (each with the specified <size>)
                                 should be read
                                 <address> should be a hexadecimal value
    rv/<size>[count] <address> - tries to read some data at the specified virtual address
                                 by default, 128 bytes of data will be read
                                 <size> can be `b` (8-bits), `w` (16-bits), `d` (32-bits),
                                 or `q` (64-bits)
                                 [count] is how many elements (each with the specified <size>)
                                 should be read
                                 <address> should be a hexadecimal value
"""
    interface.KdPrint(
        interface.KD_DEST_COMMAND,
        interface.KD_TYPE_INFO,
        f"showing the help dialog\n{HelpText.strip()}\n")

#--------------------------------------------------------------------------------------------------
# PURPOSE:
#     This function handles sending a `read memory` request to the kernel.
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
def KdpHandleReadMemoryRequest(
    Socket: socket.socket,
    DebuggeeProtocolAddress: str,
    DebuggeePort: int,
    InputTokens: list[str]) -> None:
    RequestName = InputTokens[0][:2]

    try:
        # r/AB C
        #     A -> b(yte) / w(ord) / d(word) / q(word).
        #     B -> Unsigned number; How many bytes/words/dwords/qwords to try reading.
        #     C -> Address.
        if len(InputTokens) != 2 or \
           len(InputTokens[0]) < 4 or \
           not InputTokens[0].startswith(f"{RequestName}/"):
            raise ValueError(f"expected format: {RequestName}/<size>[count] <address>")

        ItemSize = InputTokens[0][3]
        SizeMap = {"b": 1, "w": 2, "d": 4, "q": 8}
        if ItemSize not in SizeMap:
            raise ValueError(f"expected format: {RequestName}/<size>[count] <address>")
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

    if RequestName == "rp":
        protocol.KdpCurrentState = protocol.KDP_STATE_READ_PHYSICAL
        Packet = struct.pack(
            protocol.KDP_DEBUG_PACKET_READ_PHYSICAL_FORMAT,
            protocol.KDP_DEBUG_PACKET_READ_PHYSICAL_REQ,
            Address,
            ItemSize,
            ItemCount,
            Length)
    else:
        protocol.KdpCurrentState = protocol.KDP_STATE_READ_VIRTUAL
        Packet = struct.pack(
            protocol.KDP_DEBUG_PACKET_READ_VIRTUAL_FORMAT,
            protocol.KDP_DEBUG_PACKET_READ_VIRTUAL_REQ,
            Address,
            ItemSize,
            ItemCount,
            Length)

    Socket.sendto(Packet, (DebuggeeProtocolAddress, DebuggeePort))

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
#     True/False depending on if we want to terminate the execution.
#--------------------------------------------------------------------------------------------------
def KdHandleInput(
    Socket: socket.socket,
    DebuggeeProtocolAddress: str,
    DebuggeePort: int,
    InputLine: str) -> None:
    InputTokens = InputLine.split()

    if InputTokens[0] == "el" or InputTokens[0].startswith("el/"):
        KdpHandleExportLogRequest(InputTokens)
    elif InputTokens[0] == "h" or InputTokens[0] == "help":
        KdpHandleHelpRequest()
    elif InputTokens[0] == "q" or InputTokens[0] == "quit":
        return True
    elif InputTokens[0] == "rp" or InputTokens[0].startswith("rp/") or \
         InputTokens[0] == "rv" or InputTokens[0].startswith("rv/"):
        KdpHandleReadMemoryRequest(Socket, DebuggeeProtocolAddress, DebuggeePort, InputTokens)
    else:
        interface.KdPrint(
            interface.KD_DEST_COMMAND,
            interface.KD_TYPE_ERROR,
            f"invalid command `{InputLine}`\n")

    return False
