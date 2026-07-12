# SPDX-FileCopyrightText: (C) 2025 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

"""Curses-only help and log-export presentation helpers."""

from . import interface


def KdpHandleExportLogRequest(InputTokens: list[str]) -> None:
    """Export the selected curses log after command parsing has validated the request."""
    if InputTokens[0].startswith("el/"):
        Target = {
            "k": interface.KD_DEST_KERNEL,
            "c": interface.KD_DEST_COMMAND,
        }[InputTokens[0][3]]
    else:
        Target = interface.KdpSelectedOutput
    interface.KdExportBuffer(Target, InputTokens[1])


def KdpHandleHelpRequest() -> None:
    """Display the TUI controls and shared debugger command grammar."""
    HelpText = """
keyboard controls:
    tab                        - swap focus between the kernel and output logs
    up/down                    - scroll the focused log one line up/down
    page up/down               - scroll the focused log one page up/down
    ctrl+c                     - closes this application

mouse controls:
    scroll up/down             - scroll the focused log up/down

commands:
    dp[/count] <address>       - disassemble physical memory
    dv[/count] <address>       - disassemble virtual memory
    el[/target] <path>         - export the focused, kernel (`k`), or command (`c`) log
    h | help                   - show this help dialog
    ip/<size> <address>        - read an 8-, 16-, or 32-bit port value
    q | quit                   - close the debugger
    rp/<size>[count] <address> - read physical memory
    rv/<size>[count] <address> - read virtual memory

Memory sizes are `b`, `w`, `d`, or `q`; port sizes are `b`, `w`, or `d`. Addresses are hexadecimal.
Memory reads default to 128 bytes and disassembly defaults to 128 bytes.
"""
    interface.KdPrint(
        interface.KD_DEST_COMMAND,
        interface.KD_TYPE_NONE,
        f"{HelpText.strip()}\n")
