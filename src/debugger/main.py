# SPDX-FileCopyrightText: (C) 2025 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

"""Debugger command-line entry point."""

import argparse
import ipaddress
import sys
import time


def _positive(value: str) -> float:
    try:
        number = float(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("timeout must be a number") from error
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
    parser.add_argument("endpoint", help="IPv4/localhost UDP endpoint, or `serial`/unix:/path")
    parser.add_argument("port", nargs="?", help="UDP port, or serial endpoint after `serial`")
    parser.add_argument("port2", nargs="?", help="port when using `udp HOST PORT` spelling")
    parser.add_argument("--transport", choices=("udp", "serial"),
                        help="transport (default: inferred from endpoint)")
    parser.add_argument("--serial-baudrate", type=int, default=115200, metavar="BAUD")
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
    parser.add_argument("--wait-output", metavar="REGEX",
                        help="wait for matching target output before executing commands")
    parser.add_argument("--ansi", choices=("auto", "always", "never"), default="auto")
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = _parser()
    arguments = parser.parse_args(argv)
    endpoint = arguments.endpoint
    # Preserve the old namespace spelling for embedding callers while exposing transport-neutral
    # endpoint terminology in the help text.
    arguments.ip = endpoint
    if endpoint == "udp" and arguments.port is not None and arguments.port2 is not None:
        endpoint, arguments.port = arguments.port, arguments.port2
        arguments.port2 = None
        transport_name = "udp"
    elif endpoint == "serial" and arguments.port is not None and arguments.port2 is None:
        endpoint, arguments.port = arguments.port, None
        transport_name = "serial"
    else:
        transport_name = arguments.transport or ("serial" if endpoint.startswith(("unix:", "serial:")) else "udp")
        if arguments.port2 is not None:
            parser.error("unexpected third endpoint argument")
    if transport_name == "udp":
        try:
            arguments.port = _port(arguments.port) if arguments.port is not None else None
        except argparse.ArgumentTypeError as error:
            parser.error(str(error))
        if arguments.port is None:
            parser.error("UDP endpoints require a port")
        if endpoint != "localhost":
            try:
                ipaddress.IPv4Address(endpoint)
            except ValueError:
                parser.error(f"invalid IPv4 address: {endpoint}")
    elif arguments.port is not None:
        parser.error("serial endpoints do not accept a UDP port")
    headless_options = (arguments.format is not None or arguments.script or arguments.command or
                        arguments.wait_output)
    if not arguments.headless and headless_options:
        parser.error("--format, --script, and --command require --headless")
    if arguments.headless:
        lines: list[str] = []
        try:
            if arguments.script == "-" or (arguments.script is None and not arguments.command and transport_name != "serial"):
                lines.extend(sys.stdin.readlines())
            elif arguments.script:
                with open(arguments.script, encoding="utf-8") as script:
                    lines.extend(script.readlines())
            lines.extend(arguments.command)
        except (OSError, UnicodeError) as error:
            print(f"error: {error}", file=sys.stderr)
            return 1
        from .headless import run
        host = endpoint if transport_name == "udp" else ("serial:" + endpoint if not endpoint.startswith("unix:") and not endpoint.startswith("serial:") else endpoint)
        return run(host, arguments.port, lines, arguments.format or "text", arguments.ansi,
                   arguments.connect_timeout, arguments.stop_timeout, arguments.request_timeout,
                   baudrate=arguments.serial_baudrate, wait_output=arguments.wait_output)
    return _tui(endpoint, arguments.port, transport_name, arguments.serial_baudrate)


def _tui(endpoint: str, port: int | None, transport_name: str = "udp",
         baudrate: int = 115200) -> int:
    # Imports stay here so headless operation does not require curses (or capstone).
    from . import command as legacy_command
    from . import interface, utils
    from .commands import CommandError, LocalCommand, parse
    from .session import Session, SessionError
    from .transport import TransportTimeout, open_transport

    try:
        if transport_name == "udp":
            transport = open_transport(endpoint, port)
        else:
            serial_endpoint = endpoint if endpoint.startswith(("unix:", "serial:")) else "serial:" + endpoint
            transport = open_transport(serial_endpoint, baudrate=baudrate)
    except Exception as error:
        print(f"error: failed to create the debugger transport: {error}")
        return 1

    try:
        interface.KdpInitializeInterface()
    except BaseException:
        transport.close()
        raise
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
        # v1 exposes status and an explicit stop request while the target is running.
        allow_input = True
        input_line = ""
        pending_deadline = None
        while True:
            prompt_state, input_line, input_finished = interface.KdReadInput(
                prompt_state, allow_input, input_line, "kd> ")
            if input_finished:
                interface.KdPrint(interface.KD_DEST_COMMAND, interface.KD_TYPE_NONE,
                                  f"kd> {input_line}\n")
                try:
                    parsed = parse(input_line)
                    if isinstance(parsed, LocalCommand):
                        if parsed.name in ("quit", "detach"):
                            return 0
                        if parsed.name == "help":
                            legacy_command.KdpHandleHelpRequest()
                        elif parsed.name == "export":
                            target, path = parsed.arguments
                            token = "el" + (f"/{target}" if target else "")
                            legacy_command.KdpHandleExportLogRequest([token, path])
                    elif parsed is not None:
                        session.begin(parsed)
                        pending_deadline = time.monotonic() + 5
                except (CommandError, SessionError) as error:
                    interface.KdPrint(interface.KD_DEST_COMMAND, interface.KD_TYPE_NONE,
                                      f"{error}\n")
                input_line = ""
            try:
                for item in session.poll(0.01):
                    if _render_tui_event(item, interface, utils, session.architecture):
                        allow_input = True
                    if item.kind.endswith("_result"):
                        pending_deadline = None
            except TransportTimeout:
                pass
            if pending_deadline is not None and time.monotonic() >= pending_deadline:
                session.pending = None
                pending_deadline = None
                interface.KdPrint(interface.KD_DEST_COMMAND, interface.KD_TYPE_NONE,
                                  "timed out waiting for target\n")
    except KeyboardInterrupt:
        return 0
    except (OSError, SessionError) as error:
        interface.KdPrint(interface.KD_DEST_COMMAND, interface.KD_TYPE_NONE, f"error: {error}\n")
        return 1
    finally:
        try:
            interface.KdpShutdownInterface()
        finally:
            transport.close()


def _render_tui_event(item, interface, utils, architecture) -> bool:
    if item.kind == "target_output":
        interface.KdPrint(interface.KD_DEST_KERNEL, interface.KD_TYPE_NONE, item.data["text"])
    elif item.kind == "stop":
        interface.KdPrint(interface.KD_DEST_COMMAND, interface.KD_TYPE_NONE,
                          f"target stopped (reason {item.data['reason']})\n")
        return True
    elif item.kind == "diagnostic":
        interface.KdPrint(interface.KD_DEST_COMMAND, interface.KD_TYPE_NONE,
                          f"{item.data['level']}: {item.data['message']}\n")
    elif item.kind == "memory_result":
        data = item.data
        if not data["item_size"]:
            interface.KdPrint(interface.KD_DEST_COMMAND, interface.KD_TYPE_NONE,
                              f"failed to read the specified {data['space']} memory range\n")
        elif data["disassemble"]:
            payload = bytes.fromhex(data["payload"])
            try:
                utils.KdpPrintDisassemblyData(data["space"], payload, data["address"],
                                              len(payload), architecture)
            except ImportError:
                interface.KdPrint(interface.KD_DEST_COMMAND, interface.KD_TYPE_NONE,
                                  "capstone is required for disassembly\n")
        else:
            utils.KdpPrintMemoryData(data["space"], bytes.fromhex(data["payload"]),
                                     data["address"], data["item_size"],
                                     len(bytes.fromhex(data["payload"])))
    elif item.kind == "port_result":
        data = item.data
        message = ("failed to read the specified port\n" if not data["item_size"] else
                   f"{data['address']:04x}: {data['value']:0{data['item_size'] * 2}x}\n")
        interface.KdPrint(interface.KD_DEST_COMMAND, interface.KD_TYPE_NONE, message)
    elif item.kind == "status_result":
        interface.KdPrint(interface.KD_DEST_COMMAND, interface.KD_TYPE_NONE,
                          f"target state={item.data['target_state']}\n")
    elif item.kind == "context_result":
        interface.KdPrint(interface.KD_DEST_COMMAND, interface.KD_TYPE_NONE,
                          f"rip={item.data['context']['rip']:016x}\n")
    elif item.kind == "cpus_result":
        interface.KdPrint(interface.KD_DEST_COMMAND, interface.KD_TYPE_NONE,
                          f"processors={len(item.data['cpus'])}\n")
    elif item.kind == "breakpoints_result":
        for entry in item.data["breakpoints"]:
            interface.KdPrint(interface.KD_DEST_COMMAND, interface.KD_TYPE_NONE,
                              f"bp {entry['id']}: {entry['address']:016x} "
                              f"flags=0x{entry['flags']:x} hits={entry['hit_count']}\n")
    elif item.kind == "breakpoint_result":
        interface.KdPrint(interface.KD_DEST_COMMAND, interface.KD_TYPE_NONE,
                          f"breakpoint {item.data['action']}\n")
    elif item.kind == "execution_result":
        interface.KdPrint(interface.KD_DEST_COMMAND, interface.KD_TYPE_NONE,
                          f"target {item.data['action']} requested\n")
    return False
