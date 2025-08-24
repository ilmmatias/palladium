#!/usr/bin/python3

# SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

import socket
import struct
import sys

# Definitions related to our custom debugger protocol
KDP_DEBUG_PACKET_CONNECT_REQ = 0
KDP_DEBUG_PACKET_CONNECT_ACK = 1
KDP_DEBUG_PACKET_PRINT = 2

# Format for the custom debugger protocol structure
KDP_DEBUG_PACKET_FORMAT = '<B'

# ANSI color codes
KDP_ANSI_FG_RED = "\033[38;5;196m"
KDP_ANSI_FG_GREEN = "\033[38;5;46m"
KDP_ANSI_FG_YELLOW = "\033[38;5;226m"
KDP_ANSI_FG_BLUE = "\033[38;5;51m"

KDP_ANSI_RESET = "\033[0m"

#--------------------------------------------------------------------------------------------------
# PURPOSE:
#     This function serves as the entry point for the debugger.
#
# PARAMETERS:
#     None.
#
# RETURN VALUE:
#     None.
#--------------------------------------------------------------------------------------------------
def main():
    # Start by grabbing the debuggee's IP+port from the command line.
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <ip> <port>")
        sys.exit(1)

    debuggee_ip = sys.argv[1]
    debuggee_port = int(sys.argv[2])

    # Create the UDP socket and attempt doing a handshake with the debuggee
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(1.0)

    print(f"{KDP_ANSI_FG_BLUE}* info: {KDP_ANSI_RESET}"
          f"attempting connection with {debuggee_ip}:{debuggee_port}")

    connected = False
    while not connected:
        try:
            # Send a REQ
            packet = struct.pack(KDP_DEBUG_PACKET_FORMAT, KDP_DEBUG_PACKET_CONNECT_REQ)
            sock.sendto(packet, (debuggee_ip, debuggee_port))

            # And wait for an ACK
            data, _ = sock.recvfrom(1000)
            packet_type = struct.unpack(KDP_DEBUG_PACKET_FORMAT, data[:1])[0]
            if packet_type == KDP_DEBUG_PACKET_CONNECT_ACK:
                print(f"{KDP_ANSI_FG_BLUE}* info: {KDP_ANSI_RESET}connected with the target system")
                connected = True
        except socket.timeout:
            pass
        except Exception as e:
            print(f"{KDP_ANSI_FG_RED}* error: {KDP_ANSI_RESET}{e}")
            sys.exit(1)

    # Now we just loop while receiving the only other supported message (PRINT)
    # TODO: We should probably add a periodic ping to check if the connection is still active?
    while True:
        try:
            data, _ = sock.recvfrom(1500)
            packet_type = struct.unpack(KDP_DEBUG_PACKET_FORMAT, data[:1])[0]
            if packet_type == KDP_DEBUG_PACKET_PRINT:
                message = data[1:].decode('utf-8')
                print(f"{message.strip()}")
        except socket.timeout:
            pass
        except Exception as e:
            print(f"{KDP_ANSI_FG_RED}* error: {KDP_ANSI_RESET}{e}")
            break

if __name__ == "__main__":
    main()    
