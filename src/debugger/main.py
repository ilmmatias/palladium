# SPDX-FileCopyrightText: (C) 2025 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

"""Debugger command-line entry point."""

import argparse
import ipaddress
import sys
import time


def _positive(value: str) -> float:
    number = float(value)
    if number <= 0:
        raise argparse.ArgumentTypeError("timeout must be positive")
    return number


def _port(value: str) -> int:
    try:
        port = int(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("port must be an integer") from error
    if port not in range(65536):
        raise argparse.ArgumentTypeError("port must be between 0 and 65535")
    return port


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Kernel debugger client for Palladium")
    parser.add_argument("ip", help="IPv4 address or localhost of the debuggee")
    parser.add_argument("port", type=_port, help="UDP port of the debuggee")
    parser.add_argument("--headless", action="store_true", help="use the non-curses frontend")
    parser.add_argument("--format", choices=("text", "jsonl"),
                        help="headless output format (default: text)")
    sources = parser.add_mutually_exclusive_group()
    sources.add_argument("--script", metavar="PATH",
                         help="read commands from a UTF-8 script, or '-' for standard input")
    sources.add_argument("--command", action="append", default=[], metavar="COMMAND",
                         help="execute a command (repeatable)")
    parser.add_argument("--connect-timeout", type=_positive, default=10.0, metavar="SECONDS")
    parser.add_argument("--stop-timeout", type=_positive, default=30.0, metavar="SECONDS")
    parser.add_argument("--request-timeout", type=_positive, default=5.0, metavar="SECONDS")
    parser.add_argument("--ansi", choices=("auto", "always", "never"), default="auto")
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = _parser()
    arguments = parser.parse_args(argv)
    if arguments.ip != "localhost":
        try:
            ipaddress.IPv4Address(arguments.ip)
        except ValueError:
            parser.error(f"invalid IPv4 address: {arguments.ip}")
    headless_options = arguments.format is not None or arguments.script or arguments.command
    if not arguments.headless and headless_options:
        parser.error("--format, --script, and --command require --headless")
    if arguments.headless:
        lines: list[str] = []
        try:
            if arguments.script == "-" or (arguments.script is None and not arguments.command):
                lines.extend(sys.stdin.readlines())
            elif arguments.script:
                with open(arguments.script, encoding="utf-8") as script:
                    lines.extend(script.readlines())
            lines.extend(arguments.command)
        except (OSError, UnicodeError) as error:
            print(f"error: {error}", file=sys.stderr)
            return 1
        from .headless import run
        return run(arguments.ip, arguments.port, lines, arguments.format or "text", arguments.ansi,
                   arguments.connect_timeout, arguments.stop_timeout, arguments.request_timeout)
    return _tui(arguments.ip, arguments.port)


def _tui(address: str, port: int) -> int:
    # Imports stay here so headless operation does not require curses (or capstone).
    from . import command as legacy_command
    from . import interface, utils
    from .commands import CommandError, LocalCommand, parse
    from .session import Session, SessionError
    from .transport import TransportTimeout, UdpTransport

    try:
        transport = UdpTransport(address, port)
    except Exception as error:
        print(f"error: failed to create the UDP socket: {error}")
        return 1

    interface.KdpInitializeInterface()
    session = Session(transport)
    try:
        while True:
            interface.KdReadInput(False, False, "", "")
            try:
                connection_events = session.connect(0.01)
                break
            except TransportTimeout:
                continue
        for item in connection_events:
            _render_tui_event(item, interface, utils, session.architecture)
        interface.KdChangeInputMessage("target system is running...")
        prompt_state = False
        allow_input = False
        input_line = ""
        pending_deadline = None
        while True:
            prompt_state, input_line, input_finished = interface.KdReadInput(
                prompt_state, allow_input, input_line, "kd> ")
            if input_finished:
                interface.KdPrint(
                    interface.KD_DEST_COMMAND, interface.KD_TYPE_NONE, f"kd> {input_line}\n")
                try:
                    parsed = parse(input_line)
                    if isinstance(parsed, LocalCommand):
                        if parsed.name == "quit":
                            return 0
                        if parsed.name == "help":
                            legacy_command.KdpHandleHelpRequest()
                        else:
                            target, path = parsed.arguments
                            token = "el" + (f"/{target}" if target else "")
                            legacy_command.KdpHandleExportLogRequest([token, path])
                    elif parsed is not None:
                        session.begin(parsed)
                        pending_deadline = time.monotonic() + 5
                except (CommandError, SessionError) as error:
                    interface.KdPrint(
                        interface.KD_DEST_COMMAND, interface.KD_TYPE_NONE, f"{error}\n")
                input_line = ""
            try:
                for item in session.poll(0.01):
                    if _render_tui_event(item, interface, utils, session.architecture):
                        allow_input = True
                    if item.kind in ("memory_result", "port_result"):
                        pending_deadline = None
            except TransportTimeout:
                pass
            if pending_deadline is not None and time.monotonic() >= pending_deadline:
                session.pending = None
                pending_deadline = None
                interface.KdPrint(
                    interface.KD_DEST_COMMAND, interface.KD_TYPE_NONE,
                    "timed out waiting for target\n")
    except KeyboardInterrupt:
        return 0
    except (OSError, SessionError) as error:
        interface.KdPrint(interface.KD_DEST_COMMAND, interface.KD_TYPE_NONE, f"error: {error}\n")
        return 1
    finally:
        interface.KdpShutdownInterface()
        transport.close()


def _render_tui_event(item, interface, utils, architecture) -> bool:
    if item.kind == "target_output":
        interface.KdPrint(interface.KD_DEST_KERNEL, interface.KD_TYPE_NONE, item.data["text"])
    elif item.kind == "stop":
        return True
    elif item.kind == "memory_result":
        data = item.data
        if not data["item_size"]:
            interface.KdPrint(
                interface.KD_DEST_COMMAND, interface.KD_TYPE_NONE,
                f"failed to read the specified {data['space']} memory range\n")
        elif data["disassemble"]:
            payload = bytes.fromhex(data["payload"])
            try:
                utils.KdpPrintDisassemblyData(
                    data["space"], payload, data["address"], len(payload), architecture)
            except ImportError:
                interface.KdPrint(
                    interface.KD_DEST_COMMAND, interface.KD_TYPE_NONE,
                    "capstone is required for disassembly\n")
        else:
            payload = bytes.fromhex(data["payload"])
            utils.KdpPrintMemoryData(
                data["space"], payload, data["address"], data["item_size"], len(payload))
    elif item.kind == "port_result":
        data = item.data
        if not data["item_size"]:
            message = "failed to read the specified port\n"
        else:
            message = f"{data['address']:016x}: {data['value']:0{data['item_size'] * 2}x}\n"
        interface.KdPrint(interface.KD_DEST_COMMAND, interface.KD_TYPE_NONE, message)
    return False
