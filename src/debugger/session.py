# SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

"""Transport-neutral v1 debugger session and request state."""

from __future__ import annotations

from dataclasses import dataclass
import secrets
import struct
import time

from . import codec
from .commands import (BreakpointCommand, ContextCommand, CpusCommand, ExecutionCommand,
                       MemoryCommand, PortCommand, StatusCommand)
from .events import Event, connection, diagnostic, event, stopped, target_output
from .transport import Transport, TransportError, TransportTimeout


class SessionError(RuntimeError):
    pass


@dataclass
class _Pending:
    request_id: int
    command: object


class Session:
    def __init__(self, transport: Transport, *, capabilities: int = (
            codec.CAP_OUTPUT | codec.CAP_READ_PHYSICAL | codec.CAP_READ_VIRTUAL |
            codec.CAP_READ_PORT | codec.CAP_STOP | codec.CAP_CONTEXT | codec.CAP_CONTINUE |
            codec.CAP_STEP | codec.CAP_SOFTWARE_BREAKPOINT | codec.CAP_SMP_CONTEXT |
            codec.CAP_RECONNECT)):
        self.transport = transport
        self.local_capabilities = capabilities
        self.session_id: int | None = None
        self.architecture: int | None = None
        self.context_version: int | None = None
        self.target_capabilities = 0
        self.processor_count = 0
        self.maximum_datagram = codec.MAX_DATAGRAM
        self.target_state: int | None = None
        self.pending: _Pending | None = None
        self.stopped = False
        self._sequence = 0
        self._request_id = 0
        self._last_received_sequence = 0
        self._connected = False
        self.last_context: dict | None = None

    @property
    def connected(self) -> bool:
        return self._connected

    def _next_sequence(self) -> int:
        if self._sequence >= (1 << 64) - 1:
            raise SessionError("debugger sequence exhausted")
        self._sequence += 1
        return self._sequence

    def _next_request(self) -> int:
        if self._request_id >= (1 << 64) - 1:
            raise SessionError("debugger request id exhausted")
        self._request_id += 1
        return self._request_id

    def _send(self, message_type: int, payload: bytes, *, request_id: int = 0,
              flags: int = codec.FLAG_REQUEST, status: int = codec.STATUS_SUCCESS) -> int:
        sequence = self._next_sequence()
        data = codec.packet(message_type, flags, payload, status=status,
                            session_id=self.session_id or 0, sequence=sequence,
                            request_id=request_id)
        if len(data) > self.maximum_datagram:
            raise SessionError("packet exceeds negotiated datagram limit")
        try:
            self.transport.send(data)
        except (OSError, TransportError) as error:
            raise SessionError(f"failed to send debugger packet: {error}") from error
        return sequence

    def connect(self, timeout: float) -> list[Event]:
        if self._connected:
            return []
        nonce = secrets.randbits(64) or 1
        request_id = self._next_request()
        sequence = self._next_sequence()
        payload = codec.encode_hello_request(
            codec.HelloRequest(nonce, self.local_capabilities, codec.MAX_DATAGRAM))
        request = codec.packet(codec.HELLO, codec.FLAG_REQUEST, payload,
                               sequence=sequence, request_id=request_id)
        retry = codec.packet(codec.HELLO, codec.FLAG_REQUEST | codec.FLAG_RETRY, payload,
                             sequence=sequence, request_id=request_id)
        try:
            self.transport.send(request)
        except (OSError, TransportError) as error:
            raise SessionError(f"failed to send debugger hello: {error}") from error
        events: list[Event] = []
        deadline = time.monotonic() + timeout
        while True:
            try:
                packet = self._receive(min(deadline, time.monotonic() + 0.5))
            except TransportTimeout:
                if time.monotonic() >= deadline:
                    raise
                try:
                    self.transport.send(retry)
                except (OSError, TransportError) as error:
                    raise SessionError(f"failed to retry debugger hello: {error}") from error
                continue
            if packet.message_type == codec.HELLO and packet.flags & codec.FLAG_RESPONSE:
                if packet.request_id != request_id:
                    raise SessionError("hello response correlation mismatch")
                if packet.status != codec.STATUS_SUCCESS:
                    raise SessionError(f"hello failed with status {packet.status}")
                response = codec.decode_hello_response(packet.payload)
                if packet.session_id == 0:
                    raise SessionError("target returned an empty session id")
                self.session_id = packet.session_id
                self._last_received_sequence = packet.sequence
                self.architecture = response.architecture
                self.context_version = response.context_version
                self.target_capabilities = response.capabilities
                self.processor_count = response.processor_count
                self.maximum_datagram = response.maximum_datagram
                self.target_state = response.target_state
                self._connected = True
                events.append(connection(session_id=packet.session_id,
                                         architecture=response.architecture,
                                         context_version=response.context_version,
                                         capabilities=response.capabilities,
                                         processor_count=response.processor_count))
                return events
            try:
                events.extend(self._unsolicited(packet))
            except codec.CodecError as error:
                raise SessionError(str(error)) from error

    def reconnect(self, timeout: float) -> list[Event]:
        self.pending = None
        self._connected = False
        self.session_id = None
        self.stopped = False
        self._last_received_sequence = 0
        self._sequence = 0
        self._request_id = 0
        self.maximum_datagram = codec.MAX_DATAGRAM
        return self.connect(timeout)

    def wait_for_stop(self, timeout: float) -> list[Event]:
        events: list[Event] = []
        deadline = time.monotonic() + timeout
        while not self.stopped:
            packet = self._receive(deadline)
            try:
                events.extend(self._unsolicited(packet))
            except codec.CodecError as error:
                raise SessionError(str(error)) from error
        return events

    def begin(self, command) -> None:
        if self.pending is not None:
            raise SessionError("a request is already pending")
        if not self._connected:
            raise SessionError("debugger session is not connected")
        try:
            message_type, payload = self._command_packet(command)
        except (codec.CodecError, ValueError) as error:
            raise SessionError(str(error)) from error
        request_id = self._next_request()
        self.pending = _Pending(request_id, command)
        try:
            self._send(message_type, payload, request_id=request_id,
                       flags=codec.FLAG_REQUEST)
        except SessionError:
            self.pending = None
            raise

    def poll(self, timeout: float) -> list[Event]:
        if self.pending is None:
            packet = self._receive(time.monotonic() + timeout)
            try:
                return self._unsolicited(packet)
            except codec.CodecError as error:
                raise SessionError(str(error)) from error
        try:
            packet = self._receive(time.monotonic() + timeout)
            if packet.flags & codec.FLAG_RESPONSE:
                if packet.session_id != self.session_id:
                    raise SessionError("response has an invalid session id")
                if packet.request_id != self.pending.request_id:
                    raise SessionError("response correlation does not match pending request")
                command = self.pending.command
                self.pending = None
                try:
                    return self._result(command, packet)
                except codec.CodecError as error:
                    raise SessionError(str(error)) from error
            try:
                return self._unsolicited(packet)
            except codec.CodecError as error:
                raise SessionError(str(error)) from error
        except (SessionError, TransportError, TransportTimeout):
            self.pending = None
            raise

    def execute(self, command, timeout: float) -> list[Event]:
        self.begin(command)
        events: list[Event] = []
        deadline = time.monotonic() + timeout
        try:
            while self.pending is not None:
                events.extend(self.poll(max(0, deadline - time.monotonic())))
            return events
        finally:
            self.pending = None

    def detach(self, timeout: float) -> list[Event]:
        """Release the target session; the transport remains owned by the caller."""
        self.begin(_DetachCommand())
        events: list[Event] = []
        deadline = time.monotonic() + timeout
        try:
            while self.pending is not None:
                events.extend(self.poll(max(0, deadline - time.monotonic())))
            self._connected = False
            self.session_id = None
            self._sequence = 0
            self._request_id = 0
            return events
        finally:
            self.pending = None

    def ping(self, timeout: float) -> list[Event]:
        """Refresh target liveness while preserving asynchronous events."""
        return self.execute(_PingCommand(), timeout)

    def _command_packet(self, command) -> tuple[int, bytes]:
        required_capability = self._required_capability(command)
        if required_capability and not self.target_capabilities & required_capability:
            raise SessionError("target does not advertise this debugger capability")
        if isinstance(command, MemoryCommand):
            kind = codec.READ_PHYSICAL if command.physical else codec.READ_VIRTUAL
            return kind, codec.encode_memory_request(command.address, command.item_size, command.count)
        if isinstance(command, PortCommand):
            return codec.READ_PORT, codec.encode_port_request(command.address, command.item_size)
        if isinstance(command, StatusCommand):
            return codec.QUERY_STATUS, b""
        if isinstance(command, CpusCommand):
            maximum_entries = (codec.MAX_PAYLOAD - 8) // 16
            return codec.LIST_CPUS, struct.pack("<II", 0, maximum_entries)
        if isinstance(command, ContextCommand):
            return codec.READ_CONTEXT, struct.pack("<II", command.processor, 0)
        if isinstance(command, ExecutionCommand):
            kinds = {"stop": codec.REQUEST_STOP, "continue": codec.CONTINUE,
                     "step": codec.SINGLE_STEP}
            try:
                return kinds[command.action], b""
            except KeyError as error:
                raise SessionError(f"unknown execution command {command.action}") from error
        if isinstance(command, BreakpointCommand):
            kinds = {"add": codec.BREAKPOINT_ADD, "list": codec.BREAKPOINT_LIST,
                     "enable": codec.BREAKPOINT_ENABLE, "disable": codec.BREAKPOINT_DISABLE,
                     "remove": codec.BREAKPOINT_REMOVE}
            try:
                kind = kinds[command.action]
            except KeyError as error:
                raise SessionError(f"unknown breakpoint command {command.action}") from error
            if command.action == "add":
                address = command.value
                if address is None:
                    if self.last_context is None:
                        raise SessionError("@rip requires a current stopped context")
                    address = self.last_context["rip"]
                payload = codec.encode_breakpoint_add(address)
            elif command.action == "list":
                maximum_entries = (codec.MAX_PAYLOAD - 8) // 24
                payload = struct.pack("<II", 0, maximum_entries)
            else:
                payload = codec.encode_breakpoint_request(command.value or 0)
            return kind, payload
        if isinstance(command, _DetachCommand):
            return codec.DETACH, b""
        if isinstance(command, _PingCommand):
            return codec.PING, b""
        raise SessionError(f"unsupported command type {type(command).__name__}")

    @staticmethod
    def _required_capability(command) -> int:
        if isinstance(command, MemoryCommand):
            return codec.CAP_READ_PHYSICAL if command.physical else codec.CAP_READ_VIRTUAL
        if isinstance(command, PortCommand):
            return codec.CAP_READ_PORT
        if isinstance(command, ContextCommand):
            return codec.CAP_SMP_CONTEXT if command.processor else codec.CAP_CONTEXT
        if isinstance(command, CpusCommand):
            return codec.CAP_SMP_CONTEXT
        if isinstance(command, ExecutionCommand):
            return {"stop": codec.CAP_STOP, "continue": codec.CAP_CONTINUE,
                    "step": codec.CAP_STEP}.get(command.action, 0)
        if isinstance(command, BreakpointCommand):
            return codec.CAP_SOFTWARE_BREAKPOINT
        return 0

    def _receive(self, deadline: float) -> codec.Packet:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TransportTimeout("timed out waiting for target")
        try:
            raw = self.transport.receive(remaining)
            if self._connected and len(raw) > self.maximum_datagram:
                raise SessionError("packet exceeds negotiated datagram limit")
            packet = codec.decode(raw)
        except codec.CodecError as error:
            raise SessionError(str(error)) from error
        if self._connected and packet.session_id != self.session_id:
            raise SessionError("packet has an invalid session id")
        if self._connected and packet.sequence <= self._last_received_sequence:
            raise SessionError("stale or replayed packet sequence")
        if packet.sequence:
            self._last_received_sequence = packet.sequence
        return packet

    def _result(self, command, packet: codec.Packet) -> list[Event]:
        if packet.status != codec.STATUS_SUCCESS:
            return [diagnostic("error", "target_status",
                                f"target rejected request with status {codec.STATUS_NAMES.get(packet.status, packet.status)}",
                                status=packet.status, request_id=packet.request_id)]
        if isinstance(command, MemoryCommand):
            address, item_size, item_count, payload = codec.decode_memory_response(packet.payload)
            if address != command.address or item_size not in (0, command.item_size):
                raise SessionError("memory response does not match pending request")
            if item_size and item_count != command.count:
                raise SessionError("memory response count does not match request")
            values = ([int.from_bytes(payload[offset:offset + item_size], "little")
                       for offset in range(0, len(payload), item_size)] if item_size else [])
            return [event("memory_result", space="physical" if command.physical else "virtual",
                           address=address, item_size=item_size, item_count=item_count,
                           disassemble=command.disassemble, architecture=self.architecture,
                           payload=payload.hex(), values=values)]
        if isinstance(command, PortCommand):
            address, item_size, value = codec.decode_port_response(packet.payload)
            if address != command.address or item_size not in (0, command.item_size):
                raise SessionError("port response does not match pending request")
            return [event("port_result", address=address, item_size=item_size, value=value)]
        if isinstance(command, StatusCommand):
            status = codec.decode_status(packet.payload)
            self.target_state = status.target_state
            self.stopped = status.target_state in (codec.TARGET_STOPPED, codec.TARGET_PANIC)
            return [event("status_result", **status.__dict__)]
        if isinstance(command, CpusCommand):
            return [event("cpus_result", cpus=codec.decode_cpu_entries(packet.payload))]
        if isinstance(command, ContextCommand):
            if len(packet.payload) != 16 + codec.CONTEXT_SIZE:
                raise SessionError("malformed context response")
            processor, reserved, generation = struct.unpack_from("<IIQ", packet.payload)
            if reserved:
                raise SessionError("malformed context response")
            context = codec.decode_context(packet.payload[16:])
            self.last_context = _context_dict(context)
            return [event("context_result", processor=processor, generation=generation,
                           extended=command.extended, context=self.last_context)]
        if isinstance(command, BreakpointCommand):
            if command.action == "list":
                return [event("breakpoints_result", breakpoints=codec.decode_breakpoint_entries(packet.payload))]
            result = {"action": command.action, "id": command.value}
            if len(packet.payload) == 24:
                identifier, flags, address, hits = struct.unpack("<IIQQ", packet.payload)
                result["entry"] = {"id": identifier, "flags": flags, "address": address,
                                    "hit_count": hits}
            elif packet.payload:
                raise SessionError("malformed breakpoint response")
            return [event("breakpoint_result", **result)]
        if isinstance(command, ExecutionCommand):
            if packet.payload:
                raise SessionError("malformed execution response")
            if command.action == "stop":
                self.stopped = False
                self.target_state = codec.TARGET_STOPPING
            elif command.action == "continue":
                self.stopped = False
                self.target_state = codec.TARGET_RUNNING
            elif command.action == "step":
                self.stopped = True
            return [event("execution_result", action=command.action, status=packet.status)]
        return []

    def _unsolicited(self, packet: codec.Packet) -> list[Event]:
        if packet.flags & codec.FLAG_RESPONSE:
            raise SessionError("unexpected response")
        if packet.message_type == codec.TARGET_OUTPUT:
            output = codec.decode_output(packet.payload)
            return [target_output(output.text, output.severity)]
        if packet.message_type == codec.STOP:
            stop = codec.decode_stop(packet.payload)
            self.stopped = True
            self.target_state = codec.TARGET_STOPPED
            context = _context_dict(stop.context)
            self.last_context = context
            return [stopped(stop.reason, processor=stop.processor, address=stop.address,
                             resumable=bool(stop.flags & codec.STOP_FLAG_RESUMABLE),
                             context=context)]
        raise SessionError("unexpected debugger packet")


DebugSession = Session


class _DetachCommand:
    pass


class _PingCommand:
    pass


def _context_dict(context: codec.Amd64Context) -> dict:
    return {
        "architecture": context.architecture,
        "version": context.version,
        "valid_fields": context.valid_fields,
        "gpr": list(context.gpr),
        "rip": context.rip,
        "rflags": context.rflags,
        "cr2": context.cr2,
        "error_code": context.error_code,
        "mxcsr": context.mxcsr,
        "cs": context.cs,
        "ss": context.ss,
        "xmm": context.xmm.hex(),
    }
