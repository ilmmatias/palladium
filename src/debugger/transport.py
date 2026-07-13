# SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

"""Bounded datagram transports used by the debugger session.

UDP is peer validated.  Serial transports use COBS-delimited frames containing a little-endian
length, one protocol datagram, and a CRC32C over the length and datagram.  The byte framing stays
here; :mod:`codec` only sees complete protocol datagrams.
"""

from __future__ import annotations

from collections.abc import Callable
import socket
import time
from typing import BinaryIO, Protocol

from .codec import MAX_DATAGRAM


class TransportTimeout(TimeoutError):
    pass


class TransportError(OSError):
    pass


class Transport(Protocol):
    def send(self, data: bytes) -> None: ...
    def receive(self, timeout: float) -> bytes: ...
    def close(self) -> None: ...


def _crc32c(data: bytes) -> int:
    crc = 0xFFFFFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0x82F63B78 if crc & 1 else 0)
    return crc ^ 0xFFFFFFFF


def cobs_encode(data: bytes) -> bytes:
    if not data:
        return b"\x01"
    output = bytearray((0,))
    code_index = 0
    code = 1
    for byte in data:
        if byte == 0:
            output[code_index] = code
            code_index = len(output)
            output.append(0)
            code = 1
        else:
            output.append(byte)
            code += 1
            if code == 0xFF:
                output[code_index] = code
                code_index = len(output)
                output.append(0)
                code = 1
    output[code_index] = code
    return bytes(output)


def cobs_decode(data: bytes) -> bytes:
    if not data:
        raise TransportError("empty COBS frame")
    output = bytearray()
    index = 0
    while index < len(data):
        code = data[index]
        if code == 0:
            raise TransportError("zero inside COBS frame")
        index += 1
        end = index + code - 1
        if end > len(data):
            raise TransportError("truncated COBS frame")
        output.extend(data[index:end])
        index = end
        if code != 0xFF and index < len(data):
            output.append(0)
    return bytes(output)


def frame_encode(datagram: bytes) -> bytes:
    if not isinstance(datagram, bytes) or len(datagram) > MAX_DATAGRAM:
        raise TransportError("datagram exceeds serial limit")
    body = len(datagram).to_bytes(2, "little") + datagram
    body += _crc32c(body).to_bytes(4, "little")
    return cobs_encode(body) + b"\0"


def frame_decode(frame: bytes) -> bytes:
    if frame.endswith(b"\0"):
        frame = frame[:-1]
    body = cobs_decode(frame)
    if len(body) < 6:
        raise TransportError("short serial frame")
    length = int.from_bytes(body[:2], "little")
    if length > MAX_DATAGRAM or len(body) != 2 + length + 4:
        raise TransportError("invalid serial frame length")
    expected = int.from_bytes(body[-4:], "little")
    if _crc32c(body[:-4]) != expected:
        raise TransportError("serial frame CRC mismatch")
    return body[2:2 + length]


class UdpTransport:
    """IPv4 datagram transport which accepts replies only from the negotiated peer."""

    def __init__(self, host: str, port: int):
        infos = socket.getaddrinfo(host, port, socket.AF_INET, socket.SOCK_DGRAM)
        if not infos:
            raise OSError("target did not resolve to IPv4")
        self.peer = infos[0][4]
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def close(self) -> None:
        self.socket.close()

    def send(self, data: bytes) -> None:
        if len(data) > MAX_DATAGRAM:
            raise TransportError("datagram exceeds UDP limit")
        self.socket.sendto(data, self.peer)

    def receive(self, timeout: float) -> bytes:
        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TransportTimeout("timed out waiting for target")
            self.socket.settimeout(remaining)
            try:
                data, peer = self.socket.recvfrom(MAX_DATAGRAM + 1)
            except socket.timeout as error:
                raise TransportTimeout("timed out waiting for target") from error
            if peer == self.peer:
                if len(data) > MAX_DATAGRAM:
                    raise TransportError("oversized UDP datagram")
                return data

    def __enter__(self):
        return self

    def __exit__(self, *_args):
        self.close()


class SerialTransport:
    """COBS/CRC32C transport over a Unix stream or a physical serial device.

    ``unix:/path`` uses only the standard library.  Other paths import ``pyserial`` lazily, so a
    headless UDP invocation does not require that optional dependency.
    """

    def __init__(self, path: str, baudrate: int = 115200, *, stream=None,
                 stream_factory: Callable[[], BinaryIO] | None = None):
        if path.startswith("serial:"):
            path = path[7:]
        self.path = path
        self._socket: socket.socket | None = None
        self._stream = stream
        self._stream_factory = stream_factory
        self._closed = False
        if stream is not None:
            return
        if stream_factory is not None:
            self._stream = stream_factory()
            return
        if path.startswith("unix:"):
            self._socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            address = path[5:]
            deadline = time.monotonic() + 2.0
            while True:
                try:
                    self._socket.connect(address)
                    break
                except (FileNotFoundError, ConnectionRefusedError):
                    if time.monotonic() >= deadline:
                        self._socket.close()
                        raise
                    time.sleep(0.01)
            self._stream = self._socket.makefile("rwb", buffering=0)
        else:
            try:
                import serial
            except ImportError as error:
                raise TransportError("pyserial is required for physical serial devices") from error
            self._stream = serial.Serial(path, baudrate=baudrate, timeout=0)

    def close(self) -> None:
        if self._closed:
            return
        self._closed = True
        if self._stream is not None:
            try:
                self._stream.close()
            except OSError:
                pass
        if self._socket is not None:
            self._socket.close()

    def _write(self, data: bytes) -> None:
        if self._stream is None:
            raise TransportError("serial stream is unavailable")
        offset = 0
        while offset < len(data):
            count = self._stream.write(data[offset:])
            if count is None:
                count = len(data) - offset
            if count <= 0:
                raise TransportError("serial stream write failed")
            offset += count
        flush = getattr(self._stream, "flush", None)
        if flush is not None:
            flush()

    def _read_byte(self, deadline: float) -> int:
        if self._stream is None:
            raise TransportError("serial stream is unavailable")
        while True:
            if time.monotonic() >= deadline:
                raise TransportTimeout("timed out waiting for serial frame")
            if self._socket is not None:
                remaining = max(0.001, deadline - time.monotonic())
                self._socket.settimeout(remaining)
                try:
                    data = self._socket.recv(1)
                except socket.timeout as error:
                    raise TransportTimeout("timed out waiting for serial frame") from error
            else:
                data = self._stream.read(1)
            if data:
                return data[0]

    def send(self, data: bytes) -> None:
        self._write(frame_encode(data))

    def receive(self, timeout: float) -> bytes:
        frame = bytearray()
        discarding = False
        deadline = time.monotonic() + timeout
        while True:
            byte = self._read_byte(deadline)
            if byte == 0:
                if discarding or not frame:
                    frame.clear()
                    discarding = False
                    continue
                try:
                    return frame_decode(bytes(frame))
                except TransportError:
                    frame.clear()
                    continue
            if discarding:
                continue
            frame.append(byte)
            if len(frame) > 2 * (MAX_DATAGRAM + 8):
                frame.clear()
                discarding = True

    def __enter__(self):
        return self

    def __exit__(self, *_args):
        self.close()


def open_transport(endpoint: str, port: int | None = None, *, baudrate: int = 115200) -> Transport:
    """Open an endpoint without importing optional serial support on UDP paths."""
    if endpoint.startswith("unix:") or endpoint.startswith("serial:"):
        return SerialTransport(endpoint[7:] if endpoint.startswith("serial:") else endpoint,
                               baudrate=baudrate)
    if port is None:
        raise TransportError("UDP endpoint requires a port")
    return UdpTransport(endpoint, port)


class UnixTransport(SerialTransport):
    def __init__(self, path: str):
        super().__init__(path if path.startswith("unix:") else "unix:" + path)


create_transport = open_transport
DatagramTransport = UdpTransport
