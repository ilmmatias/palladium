# SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

"""Peer-validating UDP transport."""

import socket
import time


class TransportTimeout(TimeoutError):
    pass


class UdpTransport:
    def __init__(self, host: str, port: int):
        infos = socket.getaddrinfo(host, port, socket.AF_INET, socket.SOCK_DGRAM)
        if not infos:
            raise OSError("target did not resolve to IPv4")
        self.peer = infos[0][4]
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def close(self) -> None:
        self.socket.close()

    def send(self, data: bytes) -> None:
        self.socket.sendto(data, self.peer)

    def receive(self, timeout: float) -> bytes:
        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TransportTimeout("timed out waiting for target")
            self.socket.settimeout(remaining)
            try:
                data, peer = self.socket.recvfrom(65535)
            except socket.timeout as error:
                raise TransportTimeout("timed out waiting for target") from error
            if peer == self.peer:
                return data

    def __enter__(self):
        return self

    def __exit__(self, *_args):
        self.close()
