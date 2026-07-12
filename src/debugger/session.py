# SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

"""Single-pending-request debugger session."""

import time

from . import codec
from .commands import MemoryCommand, PortCommand
from .events import Event, event
from .transport import TransportTimeout, UdpTransport


class SessionError(RuntimeError):
    pass


class Session:
    def __init__(self, transport: UdpTransport):
        self.transport = transport
        self.architecture: str | None = None
        self.pending: MemoryCommand | PortCommand | None = None
        self.stopped = False

    def connect(self, timeout: float) -> list[Event]:
        self.transport.send(codec.connect_request())
        events: list[Event] = []
        deadline = time.monotonic() + timeout
        while True:
            packet = self._receive(deadline)
            if isinstance(packet, codec.Connected):
                self.architecture = packet.architecture
                events.append(event("connection", state="connected",
                                    architecture=packet.architecture, protocol_version=0))
                return events
            events.extend(self._unsolicited(packet))

    def wait_for_stop(self, timeout: float) -> list[Event]:
        events: list[Event] = []
        deadline = time.monotonic() + timeout
        while True:
            packet = self._receive(deadline)
            events.extend(self._unsolicited(packet))
            if isinstance(packet, codec.Stopped):
                return events

    def begin(self, command: MemoryCommand | PortCommand) -> None:
        if self.pending is not None:
            raise SessionError("a request is already pending")
        self.pending = command
        try:
            if isinstance(command, MemoryCommand):
                self.transport.send(codec.memory_request(command.physical, command.address,
                                                         command.item_size, command.count))
            else:
                self.transport.send(codec.port_request(command.address, command.item_size))
        except codec.CodecError as error:
            self.pending = None
            raise SessionError(str(error)) from error

    def poll(self, timeout: float) -> list[Event]:
        try:
            packet = self._receive(time.monotonic() + timeout)
            if self.pending is not None and self._matches(self.pending, packet):
                result = self._result(self.pending, packet)
                self.pending = None
                return [result]
            return self._unsolicited(packet)
        except SessionError:
            self.pending = None
            raise

    def execute(self, command: MemoryCommand | PortCommand, timeout: float) -> list[Event]:
        self.begin(command)
        events: list[Event] = []
        deadline = time.monotonic() + timeout
        try:
            while self.pending is not None:
                events.extend(self.poll(max(0, deadline - time.monotonic())))
            return events
        finally:
            self.pending = None

    def _receive(self, deadline: float) -> codec.Packet:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TransportTimeout("timed out waiting for target")
        try:
            return codec.decode(self.transport.receive(remaining))
        except codec.CodecError as error:
            raise SessionError(str(error)) from error

    @staticmethod
    def _matches(command, packet) -> bool:
        return ((isinstance(command, MemoryCommand) and isinstance(packet, codec.MemoryReply)
                 and packet.physical == command.physical) or
                (isinstance(command, PortCommand) and isinstance(packet, codec.PortReply)))

    def _result(self, command, packet) -> Event:
        if isinstance(packet, codec.MemoryReply):
            if packet.address != command.address or packet.item_size not in (0, command.item_size):
                raise SessionError("memory acknowledgement does not match pending request")
            if packet.item_size and packet.item_count != command.count:
                raise SessionError("memory acknowledgement count does not match request")
            values = []
            if packet.item_size:
                values = [
                    int.from_bytes(packet.payload[Offset:Offset + packet.item_size], "little")
                    for Offset in range(0, len(packet.payload), packet.item_size)
                ]
            return event("memory_result", space="physical" if packet.physical else "virtual",
                         address=packet.address, item_size=packet.item_size,
                         item_count=packet.item_count, disassemble=command.disassemble,
                         payload=packet.payload.hex(), values=values)
        if packet.address != command.address or packet.item_size not in (0, command.item_size):
            raise SessionError("port acknowledgement does not match pending request")
        return event("port_result", address=packet.address, item_size=packet.item_size,
                     value=packet.value)

    def _unsolicited(self, packet: codec.Packet) -> list[Event]:
        if isinstance(packet, codec.Connected):
            return []
        if isinstance(packet, codec.Printed):
            return [event("target_output", text=packet.text)]
        if isinstance(packet, codec.InvalidPrint):
            return [event("diagnostic", level="error", code="protocol", message=packet.message)]
        if isinstance(packet, codec.Stopped):
            self.stopped = True
            return [event("stop", reason="break")]
        raise SessionError("unexpected acknowledgement")
