# SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

"""Pure, strict codec for the Palladium debugger protocol v1.

The codec deliberately knows nothing about sockets, serial framing, terminal output, or command
grammar.  A datagram is an exact 48-byte little-endian envelope followed by a bounded payload.
"""

from __future__ import annotations

from dataclasses import dataclass
import struct
from typing import Any

MAGIC = b"PDKD"
MAJOR = 1
MINOR = 0
HEADER_SIZE = 48
MAX_DATAGRAM = 1024
MAX_PAYLOAD = MAX_DATAGRAM - HEADER_SIZE
PROTOCOL_MAGIC = MAGIC
PROTOCOL_MAJOR = MAJOR
PROTOCOL_MINOR = MINOR
PROTOCOL_HEADER_SIZE = HEADER_SIZE
PROTOCOL_MAX_DATAGRAM = MAX_DATAGRAM
PROTOCOL_MAX_PAYLOAD = MAX_PAYLOAD

# Header flags and status values mirror kernel/include/private/kernel/detail/kdpdefs.h.
FLAG_REQUEST = 1 << 0
FLAG_RESPONSE = 1 << 1
FLAG_EVENT = 1 << 2
FLAG_RETRY = 1 << 3
PROTOCOL_FLAG_REQUEST = FLAG_REQUEST
PROTOCOL_FLAG_RESPONSE = FLAG_RESPONSE
PROTOCOL_FLAG_EVENT = FLAG_EVENT
PROTOCOL_FLAG_RETRY = FLAG_RETRY

STATUS_SUCCESS = 0
STATUS_MALFORMED_MESSAGE = 1
STATUS_UNSUPPORTED_VERSION = 2
STATUS_INVALID_SESSION = 3
STATUS_INVALID_SEQUENCE = 4
STATUS_INVALID_STATE = 5
STATUS_UNSUPPORTED = 6
STATUS_INVALID_ARGUMENT = 7
STATUS_NOT_FOUND = 8
STATUS_BUSY = 9
STATUS_ACCESS_VIOLATION = 10
STATUS_TOO_LARGE = 11
STATUS_TIMEOUT = 12
STATUS_CONFLICT = 13
STATUS_NONRESUMABLE = 14
STATUS_INTERNAL_ERROR = 15
STATUS_NAMES = {
    STATUS_SUCCESS: "success", STATUS_MALFORMED_MESSAGE: "malformed_message",
    STATUS_UNSUPPORTED_VERSION: "unsupported_version", STATUS_INVALID_SESSION: "invalid_session",
    STATUS_INVALID_SEQUENCE: "invalid_sequence", STATUS_INVALID_STATE: "invalid_state",
    STATUS_UNSUPPORTED: "unsupported", STATUS_INVALID_ARGUMENT: "invalid_argument",
    STATUS_NOT_FOUND: "not_found", STATUS_BUSY: "busy", STATUS_ACCESS_VIOLATION: "access_violation",
    STATUS_TOO_LARGE: "too_large", STATUS_TIMEOUT: "timeout", STATUS_CONFLICT: "conflict",
    STATUS_NONRESUMABLE: "nonresumable", STATUS_INTERNAL_ERROR: "internal_error",
}

HELLO = 0x0001
PING = 0x0002
DETACH = 0x0003
TARGET_OUTPUT = 0x0010
STOP = 0x0011
QUERY_STATUS = 0x0100
LIST_CPUS = 0x0101
READ_CONTEXT = 0x0102
READ_PHYSICAL = 0x0200
READ_VIRTUAL = 0x0201
READ_PORT = 0x0202
REQUEST_STOP = 0x0300
CONTINUE = 0x0301
SINGLE_STEP = 0x0302
BREAKPOINT_ADD = 0x0400
BREAKPOINT_REMOVE = 0x0401
BREAKPOINT_ENABLE = 0x0402
BREAKPOINT_DISABLE = 0x0403
BREAKPOINT_LIST = 0x0404
MSG_HELLO = HELLO
MSG_PING = PING
MSG_DETACH = DETACH
MSG_TARGET_OUTPUT = TARGET_OUTPUT
MSG_STOP = STOP
MSG_QUERY_STATUS = QUERY_STATUS
MSG_LIST_CPUS = LIST_CPUS
MSG_READ_CONTEXT = READ_CONTEXT
MSG_READ_PHYSICAL = READ_PHYSICAL
MSG_READ_VIRTUAL = READ_VIRTUAL
MSG_READ_PORT = READ_PORT
MSG_REQUEST_STOP = REQUEST_STOP
MSG_CONTINUE = CONTINUE
MSG_SINGLE_STEP = SINGLE_STEP
MSG_BREAKPOINT_ADD = BREAKPOINT_ADD
MSG_BREAKPOINT_REMOVE = BREAKPOINT_REMOVE
MSG_BREAKPOINT_ENABLE = BREAKPOINT_ENABLE
MSG_BREAKPOINT_DISABLE = BREAKPOINT_DISABLE
MSG_BREAKPOINT_LIST = BREAKPOINT_LIST

CAP_OUTPUT = 1 << 0
CAP_READ_PHYSICAL = 1 << 1
CAP_READ_VIRTUAL = 1 << 2
CAP_READ_PORT = 1 << 3
CAP_STOP = 1 << 4
CAP_CONTEXT = 1 << 5
CAP_CONTINUE = 1 << 6
CAP_STEP = 1 << 7
CAP_SOFTWARE_BREAKPOINT = 1 << 8
CAP_SMP_CONTEXT = 1 << 9
CAP_RECONNECT = 1 << 10
CAPABILITY_MASK = (1 << 11) - 1

ARCH_AMD64 = 0x8664
CONTEXT_VERSION = 1
CONTEXT_SIZE = 448

TARGET_BOOTING = 0
TARGET_RUNNING = 1
TARGET_STOPPING = 2
TARGET_STOPPED = 3
TARGET_RESUMING = 4
TARGET_PANIC = 5

STOP_REASON_MANUAL = 0
STOP_REASON_SOFTWARE_BREAKPOINT = 1
STOP_REASON_SINGLE_STEP = 2
STOP_REASON_DEBUGGER_LOST = 3
STOP_REASON_PANIC = 4
STOP_FLAG_RESUMABLE = 1 << 0
CONTEXT_VALID_GPR = 1 << 0
CONTEXT_VALID_INSTRUCTION = 1 << 1
CONTEXT_VALID_SEGMENTS = 1 << 2
CONTEXT_VALID_EXCEPTION = 1 << 3
CONTEXT_VALID_XMM = 1 << 4
CONTEXT_VALID_MASK = (1 << 5) - 1
BREAKPOINT_FLAG_ENABLED = 1 << 0
MAX_BREAKPOINTS = 64

_HEADER = struct.Struct("<4sBBHHHIIIQQQ")
_HELLO_REQUEST = struct.Struct("<QQII")
_HELLO_RESPONSE = struct.Struct("<QIHHIIIIII")
_CONTEXT = struct.Struct("<IHHQ21QHHI256s")
_STOP = struct.Struct("<IIQIIQII")
_OUTPUT = struct.Struct("<II")
_STATUS = struct.Struct("<IIQIIQII")
_LIST_REQUEST = struct.Struct("<II")
_LIST_RESPONSE = struct.Struct("<II")
_CPU = struct.Struct("<IIQ")
_CONTEXT_REQUEST = struct.Struct("<II")
_CONTEXT_RESPONSE_PREFIX = struct.Struct("<IIQ")
_MEMORY_REQUEST = struct.Struct("<QII")
_MEMORY_RESPONSE = struct.Struct("<QIIII")
_PORT_REQUEST = struct.Struct("<HBB")
_PORT_RESPONSE = struct.Struct("<HBBQ")
_BREAKPOINT_ADD = struct.Struct("<Q")
_BREAKPOINT_REQUEST = struct.Struct("<II")
_BREAKPOINT_ENTRY = struct.Struct("<IIQQ")
_MESSAGE_TYPES = {HELLO, PING, DETACH, TARGET_OUTPUT, STOP, QUERY_STATUS, LIST_CPUS,
                  READ_CONTEXT, READ_PHYSICAL, READ_VIRTUAL, READ_PORT, REQUEST_STOP,
                  CONTINUE, SINGLE_STEP, BREAKPOINT_ADD, BREAKPOINT_REMOVE, BREAKPOINT_ENABLE,
                  BREAKPOINT_DISABLE, BREAKPOINT_LIST}


class CodecError(ValueError):
    """Raised when a packet violates the v1 envelope or payload contract."""


@dataclass(frozen=True)
class Header:
    magic: bytes
    major: int
    minor: int
    header_size: int
    message_type: int
    flags: int
    total_size: int
    status: int
    reserved: int
    session_id: int
    sequence: int
    request_id: int

    @property
    def type(self) -> int:
        return self.message_type

    @property
    def total_length(self) -> int:
        return self.total_size


@dataclass(frozen=True)
class Packet:
    header: Header
    payload: bytes

    @property
    def message_type(self) -> int:
        return self.header.message_type

    @property
    def flags(self) -> int:
        return self.header.flags

    @property
    def status(self) -> int:
        return self.header.status

    @property
    def session_id(self) -> int:
        return self.header.session_id

    @property
    def sequence(self) -> int:
        return self.header.sequence

    @property
    def request_id(self) -> int:
        return self.header.request_id


DecodedPacket = Packet


@dataclass(frozen=True)
class HelloRequest:
    nonce: int
    capabilities: int
    maximum_datagram: int


@dataclass(frozen=True)
class HelloResponse:
    capabilities: int
    maximum_datagram: int
    architecture: int
    context_version: int
    processor_count: int
    target_state: int
    transport: int
    disconnect_policy: int
    disconnect_timeout_ms: int


@dataclass(frozen=True)
class Amd64Context:
    valid_fields: int
    gpr: tuple[int, ...]
    rip: int
    rflags: int
    cr2: int
    error_code: int
    mxcsr: int
    cs: int
    ss: int
    xmm: bytes = b"\0" * 256
    architecture: int = ARCH_AMD64
    version: int = CONTEXT_VERSION


@dataclass(frozen=True)
class StopEvent:
    reason: int
    flags: int
    generation: int
    processor: int
    processor_count: int
    address: int
    exception_code: int
    context: Amd64Context


@dataclass(frozen=True)
class TargetOutput:
    severity: int
    text: str


@dataclass(frozen=True)
class Status:
    target_state: int
    owner_processor: int
    generation: int
    processor_count: int
    breakpoint_count: int
    capabilities: int
    disconnect_policy: int
    disconnect_timeout_ms: int


def _u64(value: int, name: str) -> int:
    if value not in range(1 << 64):
        raise CodecError(f"{name} is outside the 64-bit range")
    return value


def _payload(payload: bytes) -> bytes:
    if not isinstance(payload, bytes):
        raise CodecError("payload must be bytes")
    if len(payload) > MAX_PAYLOAD:
        raise CodecError("payload exceeds negotiated datagram limit")
    return payload


def encode(header: Header, payload: bytes = b"") -> bytes:
    """Encode a v1 packet, requiring a complete and internally consistent header."""
    payload = _payload(payload)
    if header.magic != MAGIC or header.major != MAJOR or header.minor != MINOR:
        raise CodecError("unsupported protocol version")
    if header.header_size != HEADER_SIZE:
        raise CodecError("invalid header size")
    if header.reserved != 0:
        raise CodecError("reserved header field is nonzero")
    if header.message_type not in range(1 << 16) or header.flags not in range(1 << 16):
        raise CodecError("header field is outside its range")
    if header.status not in range(1 << 32):
        raise CodecError("status is outside the 32-bit range")
    if header.total_size != HEADER_SIZE + len(payload) or header.total_size < HEADER_SIZE:
        raise CodecError("total size does not match payload")
    if header.total_size > MAX_DATAGRAM:
        raise CodecError("packet exceeds maximum datagram size")
    for value, name in ((header.session_id, "session id"), (header.sequence, "sequence"),
                        (header.request_id, "request id")):
        _u64(value, name)
    if header.flags & FLAG_REQUEST and header.flags & FLAG_RESPONSE:
        raise CodecError("request and response flags are mutually exclusive")
    if header.flags & ~(FLAG_REQUEST | FLAG_RESPONSE | FLAG_EVENT | FLAG_RETRY):
        raise CodecError("unknown header flags")
    if header.message_type not in _MESSAGE_TYPES or header.status > STATUS_INTERNAL_ERROR:
        raise CodecError("unknown message type or status")
    if header.flags & FLAG_REQUEST and header.status != STATUS_SUCCESS:
        raise CodecError("request has a non-success status")
    return _HEADER.pack(header.magic, header.major, header.minor, header.header_size,
                        header.message_type, header.flags, header.total_size, header.status,
                        header.reserved, header.session_id, header.sequence, header.request_id) + payload


def packet(message_type: int, flags: int, payload: bytes = b"", *, status: int = STATUS_SUCCESS,
           session_id: int = 0, sequence: int = 0, request_id: int = 0) -> bytes:
    payload = _payload(payload)
    if status not in range(1 << 32):
        raise CodecError("status is outside the 32-bit range")
    return encode(Header(MAGIC, MAJOR, MINOR, HEADER_SIZE, message_type, flags,
                         HEADER_SIZE + len(payload), status, 0, session_id, sequence,
                         request_id), payload)


def decode(data: bytes) -> Packet:
    if not isinstance(data, bytes) or len(data) < HEADER_SIZE:
        raise CodecError("short datagram")
    if len(data) > MAX_DATAGRAM:
        raise CodecError("oversized datagram")
    values = _HEADER.unpack_from(data)
    header = Header(*values)
    if header.magic != MAGIC:
        raise CodecError("invalid protocol magic")
    if header.major != MAJOR or header.minor != MINOR:
        raise CodecError("unsupported protocol version")
    if header.header_size != HEADER_SIZE:
        raise CodecError("invalid header size")
    if header.reserved != 0:
        raise CodecError("reserved header field is nonzero")
    if header.total_size != len(data) or header.total_size < HEADER_SIZE:
        raise CodecError("invalid total size")
    if header.total_size > MAX_DATAGRAM:
        raise CodecError("oversized packet")
    if header.flags & FLAG_REQUEST and header.flags & FLAG_RESPONSE:
        raise CodecError("request and response flags are mutually exclusive")
    if header.flags & ~(FLAG_REQUEST | FLAG_RESPONSE | FLAG_EVENT | FLAG_RETRY):
        raise CodecError("unknown header flags")
    if header.message_type not in _MESSAGE_TYPES or header.status > STATUS_INTERNAL_ERROR:
        raise CodecError("unknown message type or status")
    if header.flags & FLAG_REQUEST and header.status != STATUS_SUCCESS:
        raise CodecError("request has a non-success status")
    return Packet(header, data[HEADER_SIZE:])


def encode_packet(packet_data: Packet) -> bytes:
    return encode(packet_data.header, packet_data.payload)


def decode_packet(data: bytes) -> Packet:
    return decode(data)


def encode_header(header: Header) -> bytes:
    """Encode only a header with an empty payload (useful for golden-vector tests)."""
    return encode(header, b"")


def decode_header(data: bytes) -> Header:
    return decode(data).header


def encode_hello_request(value: HelloRequest) -> bytes:
    if value.maximum_datagram < HEADER_SIZE or value.maximum_datagram > MAX_DATAGRAM:
        raise CodecError("invalid maximum datagram")
    if value.capabilities & ~CAPABILITY_MASK:
        raise CodecError("hello request has unknown capabilities")
    return _HELLO_REQUEST.pack(_u64(value.nonce, "nonce"), _u64(value.capabilities, "capabilities"),
                               value.maximum_datagram, 0)


def decode_hello_request(data: bytes) -> HelloRequest:
    if len(data) != _HELLO_REQUEST.size:
        raise CodecError("malformed hello request")
    nonce, caps, maximum, reserved = _HELLO_REQUEST.unpack(data)
    if reserved or maximum < HEADER_SIZE or maximum > MAX_DATAGRAM or caps & ~CAPABILITY_MASK:
        raise CodecError("invalid hello request")
    return HelloRequest(nonce, caps, maximum)


def encode_hello_response(value: HelloResponse) -> bytes:
    if value.maximum_datagram < HEADER_SIZE or value.maximum_datagram > MAX_DATAGRAM:
        raise CodecError("invalid maximum datagram")
    if value.capabilities & ~CAPABILITY_MASK:
        raise CodecError("hello response has unknown capabilities")
    return _HELLO_RESPONSE.pack(value.capabilities, value.maximum_datagram, value.architecture,
                                value.context_version, value.processor_count, value.target_state,
                                value.transport, value.disconnect_policy,
                                value.disconnect_timeout_ms, 0)


def decode_hello_response(data: bytes) -> HelloResponse:
    if len(data) != _HELLO_RESPONSE.size:
        raise CodecError("malformed hello response")
    values = _HELLO_RESPONSE.unpack(data)
    if (values[-1] or values[1] < HEADER_SIZE or values[1] > MAX_DATAGRAM or
            values[0] & ~CAPABILITY_MASK):
        raise CodecError("invalid hello response")
    return HelloResponse(*values[:-1])


encode_hello = encode_hello_request
decode_hello = decode_hello_request


def encode_context(value: Amd64Context) -> bytes:
    if len(value.gpr) != 16 or len(value.xmm) != 256:
        raise CodecError("invalid amd64 context shape")
    if value.architecture != ARCH_AMD64 or value.version != CONTEXT_VERSION or value.valid_fields & ~CONTEXT_VALID_MASK:
        raise CodecError("unsupported context schema")
    return _CONTEXT.pack(CONTEXT_SIZE, value.architecture, value.version, value.valid_fields,
                         *value.gpr, value.rip, value.rflags, value.cr2, value.error_code,
                         value.mxcsr, value.cs, value.ss, 0, value.xmm)


def decode_context(data: bytes) -> Amd64Context:
    if len(data) != CONTEXT_SIZE:
        raise CodecError("malformed amd64 context")
    values = _CONTEXT.unpack(data)
    size, arch, version, valid = values[:4]
    if (size != CONTEXT_SIZE or arch != ARCH_AMD64 or version != CONTEXT_VERSION or
            valid & ~CONTEXT_VALID_MASK or values[-2] != 0):
        raise CodecError("invalid amd64 context")
    return Amd64Context(valid, tuple(values[4:20]), *values[20:25], values[25], values[26], values[28],
                        arch, version)


def encode_stop(value: StopEvent) -> bytes:
    return _STOP.pack(value.reason, value.flags, value.generation, value.processor,
                      value.processor_count, value.address, value.exception_code, 0) + encode_context(value.context)


def decode_stop(data: bytes) -> StopEvent:
    if len(data) != _STOP.size + CONTEXT_SIZE:
        raise CodecError("malformed stop event")
    values = _STOP.unpack_from(data)
    if values[-1] != 0 or values[0] not in range(STOP_REASON_PANIC + 1):
        raise CodecError("invalid stop event")
    return StopEvent(*values[:-1], decode_context(data[_STOP.size:]))


def decode_output(data: bytes) -> TargetOutput:
    if len(data) < _OUTPUT.size:
        raise CodecError("malformed output event")
    severity, length = _OUTPUT.unpack_from(data)
    text = data[_OUTPUT.size:]
    if length != len(text):
        raise CodecError("output length does not match payload")
    try:
        return TargetOutput(severity, text.decode("utf-8"))
    except UnicodeDecodeError as error:
        raise CodecError("output is not valid UTF-8") from error


def encode_status(value: Status) -> bytes:
    return _STATUS.pack(value.target_state, value.owner_processor, value.generation,
                        value.processor_count, value.breakpoint_count, value.capabilities,
                        value.disconnect_policy, value.disconnect_timeout_ms)


def decode_status(data: bytes) -> Status:
    if len(data) != _STATUS.size:
        raise CodecError("malformed status response")
    values = _STATUS.unpack(data)
    if values[0] > TARGET_PANIC:
        raise CodecError("invalid target state")
    return Status(*values)


def encode_memory_request(address: int, item_size: int, item_count: int) -> bytes:
    _u64(address, "address")
    if item_size not in (1, 2, 4, 8) or item_count <= 0 or item_count >= 1 << 32:
        raise CodecError("invalid memory request size or count")
    length = item_size * item_count
    if length > MAX_PAYLOAD - _MEMORY_RESPONSE.size or address + length > 1 << 64:
        raise CodecError("memory request is out of range")
    return _MEMORY_REQUEST.pack(address, item_size, item_count)


def decode_memory_response(data: bytes) -> tuple[int, int, int, bytes]:
    if len(data) < _MEMORY_RESPONSE.size:
        raise CodecError("short memory response")
    address, item_size, item_count, length, reserved = _MEMORY_RESPONSE.unpack_from(data)
    payload = data[_MEMORY_RESPONSE.size:]
    if reserved or item_size not in (0, 1, 2, 4, 8) or length != len(payload):
        raise CodecError("inconsistent memory response")
    if item_size == 0 and (item_count != 0 or payload):
        raise CodecError("malformed failed memory response")
    if item_size and item_count * item_size != length:
        raise CodecError("memory response count does not match length")
    if length > MAX_PAYLOAD - _MEMORY_RESPONSE.size:
        raise CodecError("memory response payload is too large")
    return address, item_size, item_count, payload


def encode_port_request(port: int, size: int) -> bytes:
    if port not in range(1 << 16) or size not in (1, 2, 4):
        raise CodecError("invalid port request")
    return _PORT_REQUEST.pack(port, size, 0)


def decode_port_response(data: bytes) -> tuple[int, int, int]:
    if len(data) != _PORT_RESPONSE.size:
        raise CodecError("malformed port response")
    port, size, reserved, value = _PORT_RESPONSE.unpack(data)
    if reserved or size not in (0, 1, 2, 4) or (size and value >= 1 << (size * 8)):
        raise CodecError("invalid port response")
    return port, size, value


def encode_breakpoint_add(address: int) -> bytes:
    return _BREAKPOINT_ADD.pack(_u64(address, "address"))


def encode_breakpoint_request(identifier: int) -> bytes:
    if identifier not in range(1 << 32):
        raise CodecError("invalid breakpoint identifier")
    return _BREAKPOINT_REQUEST.pack(identifier, 0)


def decode_breakpoint_entries(data: bytes) -> list[dict[str, int]]:
    if len(data) < _LIST_RESPONSE.size:
        raise CodecError("short breakpoint list response")
    total, returned = _LIST_RESPONSE.unpack_from(data)
    payload = data[_LIST_RESPONSE.size:]
    if returned > MAX_BREAKPOINTS or returned > total or len(payload) != returned * _BREAKPOINT_ENTRY.size:
        raise CodecError("invalid breakpoint list response")
    entries = []
    for index in range(returned):
        identifier, flags, address, hits = _BREAKPOINT_ENTRY.unpack_from(payload, index * _BREAKPOINT_ENTRY.size)
        entries.append({"id": identifier, "flags": flags, "address": address, "hit_count": hits})
    return entries


def decode_breakpoint_entry(data: bytes) -> dict[str, int]:
    if len(data) != _BREAKPOINT_ENTRY.size:
        raise CodecError("malformed breakpoint entry")
    identifier, flags, address, hits = _BREAKPOINT_ENTRY.unpack(data)
    return {"id": identifier, "flags": flags, "address": address, "hit_count": hits}


def decode_cpu_entries(data: bytes) -> list[dict[str, int]]:
    if len(data) < _LIST_RESPONSE.size:
        raise CodecError("short CPU list response")
    total, returned = _LIST_RESPONSE.unpack_from(data)
    payload = data[_LIST_RESPONSE.size:]
    if returned > total or len(payload) != returned * _CPU.size:
        raise CodecError("invalid CPU list response")
    return [{"processor": p, "flags": f, "generation": g}
            for p, f, g in (_CPU.unpack_from(payload, i * _CPU.size) for i in range(returned))]


def request_payload(message_type: int, value: Any = None) -> bytes:
    if message_type == HELLO:
        if not isinstance(value, HelloRequest):
            raise CodecError("hello requires HelloRequest")
        return encode_hello_request(value)
    if message_type in (READ_PHYSICAL, READ_VIRTUAL):
        if not isinstance(value, tuple) or len(value) != 3:
            raise CodecError("memory request requires address, size, count")
        return encode_memory_request(*value)
    if message_type == READ_PORT:
        if not isinstance(value, tuple) or len(value) != 2:
            raise CodecError("port request requires port and size")
        return encode_port_request(*value)
    if message_type == BREAKPOINT_ADD:
        return encode_breakpoint_add(int(value))
    if message_type in (BREAKPOINT_REMOVE, BREAKPOINT_ENABLE, BREAKPOINT_DISABLE):
        return encode_breakpoint_request(int(value))
    return b""


class Codec:
    """Small stateless facade for callers preferring an object-oriented codec boundary."""

    encode = staticmethod(encode)
    decode = staticmethod(decode)
    packet = staticmethod(packet)
    encode_packet = staticmethod(encode_packet)
    decode_packet = staticmethod(decode_packet)
