# SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

"""Strict codec for the temporary version-0 kernel debugger protocol."""

from dataclasses import dataclass
import re
import struct
from typing import Union

CONNECT_REQ = 0x00
PRINT = 0x01
BREAK = 0x02
READ_PHYSICAL_REQ = 0x03
READ_VIRTUAL_REQ = 0x04
READ_PORT_REQ = 0x05
CONNECT_ACK = 0x80
READ_PHYSICAL_ACK = 0x83
READ_VIRTUAL_ACK = 0x84
READ_PORT_ACK = 0x85

_MEMORY = struct.Struct("<BQBLL")
_PORT_REQUEST = struct.Struct("<BQB")
_PORT_REPLY = struct.Struct("<BQBL")
MAX_DATAGRAM = 1024
MAX_MEMORY_PAYLOAD = MAX_DATAGRAM - _MEMORY.size


class CodecError(ValueError):
    pass


@dataclass(frozen=True)
class Connected:
    architecture: str


@dataclass(frozen=True)
class Printed:
    text: str


@dataclass(frozen=True)
class InvalidPrint:
    message: str


@dataclass(frozen=True)
class Stopped:
    pass


@dataclass(frozen=True)
class MemoryReply:
    physical: bool
    address: int
    item_size: int
    item_count: int
    payload: bytes


@dataclass(frozen=True)
class PortReply:
    address: int
    item_size: int
    value: int


Packet = Union[Connected, Printed, InvalidPrint, Stopped, MemoryReply, PortReply]


def connect_request() -> bytes:
    return bytes((CONNECT_REQ,))


def memory_request(physical: bool, address: int, item_size: int, count: int) -> bytes:
    if item_size not in (1, 2, 4, 8) or count <= 0:
        raise CodecError("invalid memory request size or count")
    length = item_size * count
    if (address not in range(1 << 64) or address + length > 1 << 64 or
            length > MAX_MEMORY_PAYLOAD or
            length >= 1 << 32 or count >= 1 << 32):
        raise CodecError("memory request is out of range")
    return _MEMORY.pack(READ_PHYSICAL_REQ if physical else READ_VIRTUAL_REQ,
                        address, item_size, count, length)


def port_request(address: int, item_size: int) -> bytes:
    if item_size not in (1, 2, 4) or address not in range(1 << 64):
        raise CodecError("invalid port request")
    return _PORT_REQUEST.pack(READ_PORT_REQ, address, item_size)


def decode(data: bytes) -> Packet:
    if not data:
        raise CodecError("empty datagram")
    if len(data) > MAX_DATAGRAM:
        raise CodecError("oversized datagram")
    kind = data[0]
    if kind == CONNECT_ACK:
        if len(data) != 17 or data[-1] != 0:
            raise CodecError("malformed connect acknowledgement")
        architecture_data = data[1:]
        terminator = architecture_data.find(b"\0")
        if terminator < 1 or any(architecture_data[terminator:]):
            raise CodecError("invalid architecture name")
        try:
            architecture = architecture_data[:terminator].decode("ascii")
        except UnicodeDecodeError as error:
            raise CodecError("invalid architecture name") from error
        if not re.fullmatch(r"[A-Za-z0-9._-]+", architecture):
            raise CodecError("invalid architecture name")
        return Connected(architecture)
    if kind == PRINT:
        try:
            return Printed(data[1:].decode("utf-8"))
        except UnicodeDecodeError:
            return InvalidPrint("print packet is not valid UTF-8")
    if kind == BREAK:
        if len(data) != 1:
            raise CodecError("malformed break packet")
        return Stopped()
    if kind in (READ_PHYSICAL_ACK, READ_VIRTUAL_ACK):
        if len(data) < _MEMORY.size:
            raise CodecError("short memory acknowledgement")
        _, address, item_size, count, length = _MEMORY.unpack_from(data)
        payload = data[_MEMORY.size:]
        if item_size == 0:
            # v0 echoes the requested count and length on a failed read.
            if payload:
                raise CodecError("malformed failed memory acknowledgement")
        elif (item_size not in (1, 2, 4, 8) or length > MAX_MEMORY_PAYLOAD or
              count * item_size != length or len(payload) != length):
            raise CodecError("inconsistent memory acknowledgement")
        return MemoryReply(kind == READ_PHYSICAL_ACK, address, item_size, count, payload)
    if kind == READ_PORT_ACK:
        if len(data) != _PORT_REPLY.size:
            raise CodecError("malformed port acknowledgement")
        _, address, item_size, value = _PORT_REPLY.unpack(data)
        if item_size not in (0, 1, 2, 4) or (item_size and value >= 1 << (item_size * 8)):
            raise CodecError("invalid port acknowledgement")
        return PortReply(address, item_size, value)
    raise CodecError(f"unknown packet type 0x{kind:02x}")
