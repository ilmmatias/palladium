# SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

import contextlib
import importlib
import io
import json
import socket
import struct
import sys
import threading
import unittest

from src.debugger import codec
from src.debugger.commands import CommandError, MemoryCommand, PortCommand, parse
from src.debugger.events import event
from src.debugger.headless import Renderer
from src.debugger.headless import run as run_headless
from src.debugger.session import Session, SessionError
from src.debugger.transport import TransportTimeout, UdpTransport


class FakeTarget:
    def __init__(self, handler):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.bind(("127.0.0.1", 0))
        self.port = self.socket.getsockname()[1]
        self.handler = handler
        self.thread = threading.Thread(target=self._run, daemon=True)
        self.error = None

    def _run(self):
        try:
            self.handler(self.socket)
        except BaseException as error:
            self.error = error

    def __enter__(self):
        self.thread.start()
        return self

    def __exit__(self, *_args):
        self.thread.join(2)
        self.socket.close()
        if self.error:
            raise self.error


class CodecTests(unittest.TestCase):
    def test_rejects_empty_truncated_and_inconsistent_packets(self):
        for packet in (b"", b"\x80", b"\x02x", b"\x83",
                       b"\xff" * 1025, struct.pack("<BQBLL", 0x83, 1, 2, 2, 3)):
            with self.subTest(packet=packet), self.assertRaises(codec.CodecError):
                codec.decode(packet)

    def test_decodes_exact_memory_reply(self):
        packet = struct.pack("<BQBLL", 0x83, 0x1000, 1, 2, 2) + b"ab"
        self.assertEqual(codec.decode(packet), codec.MemoryReply(True, 0x1000, 1, 2, b"ab"))

    def test_enforces_target_payload_limit(self):
        with self.assertRaises(codec.CodecError):
            codec.memory_request(True, 0, 1, codec.MAX_MEMORY_PAYLOAD + 1)

    def test_invalid_print_is_a_protocol_diagnostic_packet(self):
        self.assertEqual(codec.decode(b"\x01\xff"),
                         codec.InvalidPrint("print packet is not valid UTF-8"))

    def test_rejects_trailing_data_and_invalid_architectures(self):
        for packet in (
                struct.pack("<BQBL", 0x85, 0x3f8, 1, 0x41) + b"x",
                b"\x80amd64" + b"\0" * 10 + b"x",
                b"\x80am d64" + b"\0" * 10):
            with self.subTest(packet=packet), self.assertRaises(codec.CodecError):
                codec.decode(packet)


class ParserTests(unittest.TestCase):
    def test_empty_is_noop(self):
        self.assertIsNone(parse("  \t"))

    def test_disassembly_count_grammar(self):
        self.assertEqual(parse("dp/32 ff"), MemoryCommand(True, True, 0xff, 1, 32))
        self.assertEqual(parse("dv ff"), MemoryCommand(False, True, 0xff, 1, 128))
        for command in ("dp/ ff", "dv/0 ff", "dp/x ff", "dp32 ff"):
            with self.subTest(command=command), self.assertRaises(CommandError):
                parse(command)

    def test_parser_enforces_wire_payload_limit(self):
        for command in ("dp/1007 0", "rp/q126 0"):
            with self.subTest(command=command), self.assertRaises(CommandError):
                parse(command)


class TransportSessionTests(unittest.TestCase):
    def test_wrong_peer_is_ignored(self):
        def handler(target):
            _, client = target.recvfrom(100)
            wrong = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            wrong.sendto(b"wrong", client)
            wrong.close()
            target.sendto(b"right", client)
        with FakeTarget(handler) as target, UdpTransport("127.0.0.1", target.port) as transport:
            transport.send(b"hello")
            self.assertEqual(transport.receive(1), b"right")

    def test_timeout(self):
        def handler(target):
            target.recvfrom(100)
        with FakeTarget(handler) as target, UdpTransport("127.0.0.1", target.port) as transport:
            transport.send(b"hello")
            with self.assertRaises(TransportTimeout):
                transport.receive(0.02)

    def test_synchronized_connect_stop_request(self):
        def handler(target):
            data, client = target.recvfrom(100)
            self.assertEqual(data, b"\0")
            target.sendto(b"\x80amd64" + b"\0" * 11, client)
            target.sendto(b"\x02", client)
            request, client = target.recvfrom(100)
            _, address, size, count, length = struct.unpack("<BQBLL", request)
            target.sendto(struct.pack("<BQBLL", 0x83, address, size, count, length) + b"A" * length,
                          client)
        with FakeTarget(handler) as target, UdpTransport("127.0.0.1", target.port) as transport:
            session = Session(transport)
            self.assertEqual(session.connect(1)[0].kind, "connection")
            self.assertEqual(session.wait_for_stop(1)[0].kind, "stop")
            result = session.execute(MemoryCommand(True, False, 0x1000, 1, 2), 1)
            self.assertEqual(result[0].kind, "memory_result")
            self.assertEqual(result[0].data["payload"], "4141")
            self.assertEqual(result[0].data["values"], [65, 65])

    def test_malformed_reply_clears_pending(self):
        def handler(target):
            _, client = target.recvfrom(100)
            target.sendto(b"\x83", client)
        with FakeTarget(handler) as target, UdpTransport("127.0.0.1", target.port) as transport:
            session = Session(transport)
            with self.assertRaises(SessionError):
                session.execute(MemoryCommand(True, False, 0, 1, 1), 1)
            self.assertIsNone(session.pending)

    def test_busy_state_and_timeout_clear_pending(self):
        def handler(target):
            target.recvfrom(100)
        with FakeTarget(handler) as target, UdpTransport("127.0.0.1", target.port) as transport:
            session = Session(transport)
            command = MemoryCommand(True, False, 0, 1, 1)
            session.begin(command)
            with self.assertRaises(SessionError):
                session.begin(command)
            self.assertEqual(session.pending, command)
            session.pending = None
            with self.assertRaises(TransportTimeout):
                session.execute(command, 0.02)
            self.assertIsNone(session.pending)

    def test_invalid_print_emits_diagnostic_and_is_ignored(self):
        def handler(target):
            _, client = target.recvfrom(100)
            target.sendto(b"\x80amd64" + b"\0" * 11, client)
            target.sendto(b"\x01\xff", client)
            target.sendto(b"\x02", client)
        with FakeTarget(handler) as target, UdpTransport("127.0.0.1", target.port) as transport:
            session = Session(transport)
            session.connect(1)
            events = session.wait_for_stop(1)
            self.assertEqual([item.kind for item in events], ["diagnostic", "stop"])

    def test_port_result(self):
        def handler(target):
            request, client = target.recvfrom(100)
            _, address, size = struct.unpack("<BQB", request)
            target.sendto(struct.pack("<BQBL", 0x85, address, size, 0x1234), client)
        with FakeTarget(handler) as target, UdpTransport("127.0.0.1", target.port) as transport:
            session = Session(transport)
            result = session.execute(PortCommand(0x3f8, 2), 1)
            self.assertEqual(result[0].kind, "port_result")
            self.assertEqual(result[0].data["value"], 0x1234)


class OutputAndImportTests(unittest.TestCase):
    def test_headless_jsonl_fake_target(self):
        def handler(target):
            data, client = target.recvfrom(100)
            self.assertEqual(data, b"\0")
            target.sendto(b"\x80amd64" + b"\0" * 11, client)
            target.sendto(b"\x01hello from target\n", client)
            target.sendto(b"\x02", client)
            request, client = target.recvfrom(100)
            _, address, size, count, length = struct.unpack("<BQBLL", request)
            target.sendto(struct.pack("<BQBLL", 0x83, address, size, count, length) + b"AB",
                          client)

        output = io.StringIO()
        with FakeTarget(handler) as target, contextlib.redirect_stdout(output):
            status = run_headless("127.0.0.1", target.port, ["rp/b2 1000"], "jsonl", "never",
                                  1, 1, 1)
        self.assertEqual(status, 0)
        events = [json.loads(line) for line in output.getvalue().splitlines()]
        self.assertEqual([item["event"] for item in events],
                         ["connection", "target_output", "stop", "command", "memory_result"])
        self.assertEqual(events[-1]["data"]["values"], [65, 66])
        self.assertEqual([item["seq"] for item in events], list(range(len(events))))

    def test_json_is_deterministic_and_versioned(self):
        stream = io.StringIO()
        renderer = Renderer("jsonl", "never", stream)
        renderer.emit(event("diagnostic", level="info", code="sample", message="first"))
        renderer.emit(event("stop", reason="break"))
        lines = stream.getvalue().splitlines()
        first = json.loads(lines[0])
        second = json.loads(lines[1])
        self.assertEqual(first["schema"], 1)
        self.assertEqual(first["seq"], 0)
        self.assertEqual(first["event"], "diagnostic")
        self.assertEqual(second["seq"], 1)
        self.assertEqual(
            lines[0],
            '{"data":{"code":"sample","level":"info","message":"first"},'
            '"event":"diagnostic","schema":1,"seq":0}')

    def test_main_imports_without_curses_or_capstone(self):
        old = {name: sys.modules.get(name) for name in ("curses", "capstone")}
        sys.modules["curses"] = None
        sys.modules["capstone"] = None
        try:
            sys.modules.pop("src.debugger.main", None)
            importlib.import_module("src.debugger.main")
        finally:
            for name, module in old.items():
                if module is None:
                    sys.modules.pop(name, None)
                else:
                    sys.modules[name] = module


if __name__ == "__main__":
    unittest.main()
