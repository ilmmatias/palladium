# SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

"""Non-curses debugger frontend."""

from __future__ import annotations

import json
import re
import sys
from collections.abc import Iterable
from pathlib import Path

from .commands import CommandError, LocalCommand, MemoryCommand, PortCommand, parse
from .events import Event, event
from .session import Session, SessionError
from .transport import TransportTimeout, UdpTransport

EXIT_SUCCESS = 0
EXIT_RUNTIME = 1
EXIT_USAGE = 2
EXIT_TIMEOUT = 3


class Renderer:
    def __init__(self, mode: str, ansi: str, stream=None):
        self.mode = mode
        self.stream = stream or sys.stdout
        self.color = ansi == "always" or (ansi == "auto" and self.stream.isatty())
        self.sequence = 0
        self.history: list[Event] = []

    def emit(self, item: Event) -> None:
        item = self._structured(item)
        self.history.append(item)
        if self.mode == "jsonl":
            value = {"schema": 1, "seq": self.sequence, "event": item.kind, "data": item.data}
            self.sequence += 1
            print(json.dumps(value, sort_keys=True, separators=(",", ":")),
                  file=self.stream, flush=True)
            return
        text = self._text(item)
        if text:
            print(text, end="" if text.endswith("\n") else "\n", file=self.stream, flush=True)

    def export(self, target: str, path: str) -> None:
        selected = self.history
        if target == "k":
            selected = [item for item in selected if item.kind == "target_output"]
        elif target == "c":
            selected = [item for item in selected if item.kind != "target_output"]
        rendered = [self._text(item) for item in selected]
        text = "".join(value + ("" if value.endswith("\n") else "\n")
                       for value in rendered if value)
        Path(path).write_text(text, encoding="utf-8")

    def _structured(self, item: Event) -> Event:
        if item.kind != "memory_result" or not item.data["disassemble"]:
            return item
        try:
            import capstone
        except ImportError:
            return event("diagnostic", level="error", code="dependency_missing",
                         message="capstone is required for disassembly")
        payload = bytes.fromhex(item.data["payload"])
        disassembler = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
        rows = []
        for address, size, mnemonic, operands in disassembler.disasm_lite(
                payload, item.data["address"]):
            offset = address - item.data["address"]
            rows.append({
                "address": address,
                "bytes": payload[offset:offset + size].hex(),
                "mnemonic": mnemonic,
                "operands": operands,
            })
        return event("disassembly_result", space=item.data["space"],
                     address=item.data["address"], rows=rows)

    def _text(self, item: Event) -> str:
        data = item.data
        if item.kind == "target_output":
            return _ansi(data["text"], self.color)
        if item.kind == "connection":
            return f"connected ({data['architecture']}, protocol v{data['protocol_version']})"
        if item.kind == "stop":
            return f"target stopped ({data['reason']})"
        if item.kind == "command":
            return f"kd> {data['text']}"
        if item.kind == "diagnostic":
            return f"{data['level']}: {data['message']}"
        if item.kind == "port_result":
            if not data["item_size"]:
                return "failed to read the specified port"
            return f"{data['address']:016x}: {data['value']:0{data['item_size'] * 2}x}"
        if item.kind == "memory_result":
            if not data["item_size"]:
                return "failed to read the specified memory range"
            return _memory_text(data)
        if item.kind == "disassembly_result":
            return "\n".join(
                f"{row['address']:016x}:\t{row['mnemonic']:<10}\t{row['operands']}"
                for row in data["rows"])
        return ""


def run(host: str, port: int, lines: Iterable[str], mode: str, ansi: str,
        connect_timeout: float, stop_timeout: float, request_timeout: float) -> int:
    renderer = Renderer(mode, ansi)
    parsed_commands: list[tuple[str, MemoryCommand | PortCommand | LocalCommand | None]] = []
    for raw_line in lines:
        line = raw_line.rstrip("\r\n")
        try:
            parsed_commands.append((line, parse(line)))
        except CommandError as error:
            renderer.emit(event("diagnostic", level="error", code="command",
                                message=str(error)))
            return EXIT_USAGE

    if any(isinstance(command, MemoryCommand) and command.disassemble
           for _, command in parsed_commands):
        try:
            import capstone  # noqa: F401 - availability is the check.
        except ImportError:
            renderer.emit(event("diagnostic", level="error", code="dependency_missing",
                                message="capstone is required for disassembly"))
            return EXIT_RUNTIME

    try:
        with UdpTransport(host, port) as transport:
            session = Session(transport)
            for item in session.connect(connect_timeout):
                renderer.emit(item)
            if any(isinstance(command, (MemoryCommand, PortCommand))
                   for _, command in parsed_commands):
                for item in session.wait_for_stop(stop_timeout):
                    renderer.emit(item)
            for line, command in parsed_commands:
                renderer.emit(event("command", text=line))
                if command is None:
                    continue
                if isinstance(command, LocalCommand):
                    if command.name == "quit":
                        return EXIT_SUCCESS
                    if command.name == "help":
                        renderer.emit(event("diagnostic", level="info", code="help",
                                            message="commands: dp dv rp rv ip el help quit"))
                        continue
                    try:
                        renderer.export(command.arguments[0], command.arguments[1])
                    except OSError as error:
                        renderer.emit(event("diagnostic", level="error", code="export",
                                            message=str(error)))
                        return EXIT_RUNTIME
                    continue
                for item in session.execute(command, request_timeout):
                    renderer.emit(item)
            return EXIT_SUCCESS
    except TransportTimeout as error:
        renderer.emit(event("diagnostic", level="error", code="timeout", message=str(error)))
        return EXIT_TIMEOUT
    except (OSError, SessionError) as error:
        renderer.emit(event("diagnostic", level="error", code="runtime", message=str(error)))
        return EXIT_RUNTIME
    except KeyboardInterrupt:
        return EXIT_SUCCESS


def _ansi(text: str, enabled: bool) -> str:
    return text if enabled else re.sub(r"\x1b\[[0-9;]*m", "", text)


def _memory_text(data: dict) -> str:
    payload = bytes.fromhex(data["payload"])
    size = data["item_size"]
    lines = []
    for offset in range(0, len(payload), 16):
        chunk = payload[offset:offset + 16]
        values = [int.from_bytes(chunk[i:i + size], "little")
                  for i in range(0, len(chunk), size) if len(chunk[i:i + size]) == size]
        body = " ".join(f"{value:0{size * 2}x}" for value in values)
        if size == 1:
            body += " |" + "".join(chr(c) if 32 <= c < 127 else "." for c in chunk) + "|"
        lines.append(f"{data['address'] + offset:016x}: {body}")
    return "\n".join(lines)
