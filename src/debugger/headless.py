# SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

"""Non-curses debugger frontend for text and schema-2 JSONL output."""

from __future__ import annotations

import json
import re
import sys
import time
from collections.abc import Iterable
from pathlib import Path

from .commands import (BreakpointCommand, CommandError, ContextCommand, CpusCommand,
                       ExecutionCommand, LocalCommand, MemoryCommand, PortCommand,
                       StatusCommand, parse)
from .events import Event, event
from .session import Session, SessionError
from .transport import TransportTimeout, open_transport

EXIT_SUCCESS = 0
EXIT_RUNTIME = 1
EXIT_USAGE = 2
EXIT_TIMEOUT = 3


class Renderer:
    def __init__(self, mode: str, ansi: str, stream=None):
        if mode not in ("text", "jsonl"):
            raise ValueError("unsupported headless format")
        self.mode = mode
        self.stream = stream or sys.stdout
        self.color = ansi == "always" or (ansi == "auto" and self.stream.isatty())
        self.sequence = 0
        self.history: list[Event] = []

    def emit(self, item: Event) -> None:
        item = self._structured(item)
        self.history.append(item)
        if self.mode == "jsonl":
            value = {"schema": 2, "seq": self.sequence, "event": item.kind, "data": item.data}
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
        if item.kind != "memory_result" or not item.data.get("disassemble"):
            return item
        if item.data.get("architecture") not in (None, 0x8664, "amd64"):
            architecture = item.data["architecture"]
            label = (f"0x{architecture:04x}" if isinstance(architecture, int) else str(architecture))
            return event("diagnostic", level="error", code="unsupported_architecture",
                         message=f"architecture {label} is unsupported")
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
            rows.append({"address": address, "bytes": payload[offset:offset + size].hex(),
                         "mnemonic": mnemonic, "operands": operands})
        return event("disassembly_result", space=item.data["space"],
                     address=item.data["address"], rows=rows)

    def _text(self, item: Event) -> str:
        data = item.data
        if item.kind == "target_output":
            return _ansi(data["text"], self.color)
        if item.kind == "connection":
            architecture = (f"0x{data['architecture']:04x}" if isinstance(data["architecture"], int)
                            else str(data["architecture"]))
            return f"connected (architecture {architecture}, protocol v{data['protocol_version']})"
        if item.kind == "stop":
            return f"target stopped (reason {data['reason']}, cpu {data['processor']})"
        if item.kind == "command":
            return f"kd> {data['text']}"
        if item.kind == "diagnostic":
            return f"{data['level']}: {data['message']}"
        if item.kind == "status_result":
            return (f"state={data['target_state']} cpu={data['owner_processor']} "
                    f"processors={data['processor_count']} breakpoints={data['breakpoint_count']}")
        if item.kind == "cpus_result":
            return "\n".join(f"cpu {cpu['processor']}: flags=0x{cpu['flags']:x} "
                               f"generation={cpu['generation']}" for cpu in data["cpus"])
        if item.kind == "context_result":
            context = data["context"]
            return f"cpu {data['processor']} rip={context['rip']:016x} rflags={context['rflags']:016x}"
        if item.kind == "breakpoints_result":
            return "\n".join(f"bp {entry['id']}: {entry['address']:016x} flags=0x{entry['flags']:x} "
                               f"hits={entry['hit_count']}" for entry in data["breakpoints"])
        if item.kind in ("breakpoint_result", "execution_result"):
            return f"{item.kind}: {data.get('action', 'ok')}"
        if item.kind == "port_result":
            if not data["item_size"]:
                return "failed to read the specified port"
            return f"{data['address']:04x}: {data['value']:0{data['item_size'] * 2}x}"
        if item.kind == "memory_result":
            if not data["item_size"]:
                return "failed to read the specified memory range"
            return _memory_text(data)
        if item.kind == "disassembly_result":
            return "\n".join(f"{row['address']:016x}:\t{row['mnemonic']:<10}\t{row['operands']}"
                               for row in data["rows"])
        return ""


def run(host: str, port: int | None, lines: Iterable[str], mode: str, ansi: str,
        connect_timeout: float, stop_timeout: float, request_timeout: float,
        *, transport_factory=None, baudrate: int = 115200,
        wait_output: str | None = None) -> int:
    renderer = Renderer(mode, ansi)
    parsed_commands: list[tuple[str, object | None]] = []
    for raw_line in lines:
        line = raw_line.rstrip("\r\n")
        try:
            parsed_commands.append((line, parse(line)))
        except CommandError as error:
            renderer.emit(event("diagnostic", level="error", code="command", message=str(error)))
            return EXIT_USAGE

    if any(isinstance(command, MemoryCommand) and command.disassemble
           for _, command in parsed_commands):
        try:
            import capstone  # noqa: F401
        except ImportError:
            renderer.emit(event("diagnostic", level="error", code="dependency_missing",
                                message="capstone is required for disassembly"))
            return EXIT_RUNTIME

    factory = transport_factory or (lambda: open_transport(host, port, baudrate=baudrate))
    try:
        opened = factory()
        with _transport_context(opened) as transport:
            session = Session(transport)
            for item in session.connect(connect_timeout):
                renderer.emit(item)
            if wait_output:
                try:
                    output_pattern = re.compile(wait_output)
                except re.error as error:
                    renderer.emit(event("diagnostic", level="error", code="wait_output",
                                        message=str(error)))
                    return EXIT_USAGE
                deadline = time.monotonic() + stop_timeout
                matched = False
                while not matched:
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        raise TransportTimeout("timed out waiting for target output")
                    try:
                        received = session.poll(min(remaining, 1.0))
                    except TransportTimeout:
                        received = session.ping(request_timeout)
                    for item in received:
                        renderer.emit(item)
                        if item.kind == "target_output" and output_pattern.search(item.data["text"]):
                            matched = True
            if not parsed_commands and host.startswith(("unix:", "serial:")):
                # A serial diagnostic/KD stream is useful without a command script.  Stay
                # attached until the bounded idle deadline, rendering asynchronous output/events.
                while True:
                    try:
                        for item in session.poll(1.0):
                            renderer.emit(item)
                    except TransportTimeout:
                        # A physical/Unix serial stream is intentionally long-lived; QEMU or the
                        # caller owns process lifetime.  Test transports can opt into one-shot
                        # behavior by setting ``idle_once``.
                        if getattr(transport, "idle_once", False):
                            return EXIT_SUCCESS
                        for item in session.ping(request_timeout):
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
                                            message="status cpus r rx rp rv dp dv ip stop continue step bp bl be bd bc detach quit"))
                        continue
                    if command.name == "detach":
                        for item in session.detach(request_timeout):
                            renderer.emit(item)
                        return EXIT_SUCCESS
                    try:
                        renderer.export(command.arguments[0], command.arguments[1])
                    except OSError as error:
                        renderer.emit(event("diagnostic", level="error", code="export",
                                            message=str(error)))
                        return EXIT_RUNTIME
                    continue
                for item in session.execute(command, request_timeout):
                    renderer.emit(item)
                if isinstance(command, ExecutionCommand) and command.action == "stop" and \
                        not session.stopped:
                    for item in session.wait_for_stop(stop_timeout):
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


class _TransportContext:
    def __init__(self, transport):
        self.transport = transport

    def __enter__(self):
        return self.transport

    def __exit__(self, *_args):
        self.transport.close()


def _transport_context(transport):
    return transport if hasattr(transport, "__enter__") else _TransportContext(transport)


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
