# SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

import contextlib
from collections import deque
import importlib
import io
import json
import struct
import sys
import unittest

from src.debugger import codec
from src.debugger.commands import (BreakpointCommand, CommandError, ContextCommand,
                                    ExecutionCommand, MemoryCommand, StatusCommand, parse)
from src.debugger.events import event
from src.debugger.headless import Renderer, run as run_headless
from src.debugger.session import Session, SessionError
from src.debugger.transport import (SerialTransport, TransportError, TransportTimeout,
                                    cobs_decode, cobs_encode, frame_decode, frame_encode)


class FakeTransport:
    """Synchronous in-memory datagram transport (used in place of a raw UDP target in CI)."""

    def __init__(self, handler=None):
        self.incoming = deque()
        self.sent = []
        self.handler = handler
        self.closed = False

    def send(self, data):
        self.sent.append(data)
        if self.handler:
            self.handler(self, data)

    def receive(self, timeout):
        if self.incoming:
            return self.incoming.popleft()
        raise TransportTimeout("timed out waiting for target")

    def close(self):
        self.closed = True

    def __enter__(self):
        return self

    def __exit__(self, *_args):
        self.close()


class FakeUnixStream:
    def __init__(self):
        self.written = bytearray()
        self.readable = bytearray()

    def write(self, data):
        self.written.extend(data)
        return len(data)

    def flush(self):
        return None

    def read(self, count):
        if not self.readable:
            return b""
        result = bytes(self.readable[:count])
        del self.readable[:count]
        return result

    def push(self, data):
        self.readable.extend(data)

    def close(self):
        return None


def _hello_response(request, *, session_id=0x1234, sequence=1):
    request_packet = codec.decode(request)
    return codec.packet(
        codec.HELLO, codec.FLAG_RESPONSE,
        codec.encode_hello_response(codec.HelloResponse(
            codec.CAP_OUTPUT | codec.CAP_READ_PHYSICAL | codec.CAP_CONTEXT,
            codec.MAX_DATAGRAM, codec.ARCH_AMD64, codec.CONTEXT_VERSION, 1,
            codec.TARGET_RUNNING, 1, 0, 0)),
        session_id=session_id, sequence=sequence, request_id=request_packet.request_id)


class CodecTests(unittest.TestCase):
    def test_header_golden_shape_and_exact_lengths(self):
        payload = codec.encode_hello_request(codec.HelloRequest(1, codec.CAP_OUTPUT, 1024))
        raw = codec.packet(codec.HELLO, codec.FLAG_REQUEST, payload, sequence=2, request_id=3)
        self.assertEqual(len(raw), 48 + 24)
        self.assertEqual(raw[:48], struct.pack(
            "<4sBBHHHIIIQQQ", b"PDKD", 1, 0, 48, codec.HELLO, codec.FLAG_REQUEST,
            72, 0, 0, 0, 2, 3))
        self.assertEqual(codec.decode(raw).payload, payload)

    def test_rejects_malformed_or_replayed_envelope_shape(self):
        raw = codec.packet(codec.PING, codec.FLAG_REQUEST, sequence=1)
        for malformed in (b"", raw[:47], raw + b"x", raw[:4] + b"NOPE" + raw[8:]):
            with self.subTest(malformed=malformed), self.assertRaises(codec.CodecError):
                codec.decode(malformed)

    def test_context_and_stop_are_fixed_size(self):
        context = codec.Amd64Context(codec.CONTEXT_VALID_GPR if hasattr(codec, "CONTEXT_VALID_GPR") else 1,
                                     tuple(range(16)), 0x1000, 0x202, 0, 0, 0, 8, 16)
        self.assertEqual(len(codec.encode_context(context)), 448)
        stop = codec.StopEvent(codec.STOP_REASON_SOFTWARE_BREAKPOINT, codec.STOP_FLAG_RESUMABLE,
                               2, 0, 1, 0x1000, 3, context)
        raw = codec.encode_stop(stop)
        self.assertEqual(len(raw), 488)
        self.assertEqual(codec.decode_stop(raw).context.rip, 0x1000)

    def test_memory_and_port_boundaries(self):
        request = codec.encode_memory_request(0x1000, 4, 2)
        self.assertEqual(request, struct.pack("<QII", 0x1000, 4, 2))
        with self.assertRaises(codec.CodecError):
            codec.encode_memory_request(0, 1, 953)
        self.assertEqual(codec.decode_port_response(struct.pack("<HBBQ", 0x3f8, 2, 0, 0x1234)),
                         (0x3f8, 2, 0x1234))


class FramingTests(unittest.TestCase):
    def test_cobs_roundtrip_and_crc_frame(self):
        for payload in (b"", b"a\0b", bytes(range(256))):
            self.assertEqual(cobs_decode(cobs_encode(payload)), payload)
            frame = frame_encode(payload)
            self.assertEqual(frame[-1], 0)
            self.assertEqual(frame_decode(frame[:-1]), payload)

    def test_frame_rejects_length_and_crc_tampering(self):
        frame = bytearray(frame_encode(b"hello")[:-1])
        frame[-1] ^= 1
        with self.assertRaises(TransportError):
            frame_decode(bytes(frame))

    def test_unix_socket_stream_uses_same_framing(self):
        stream = FakeUnixStream()
        client = SerialTransport("unix:unused", stream=stream)
        try:
            client.send(b"hello")
            self.assertEqual(frame_decode(bytes(stream.written)[:-1]), b"hello")
            stream.push(frame_encode(b"world"))
            self.assertEqual(client.receive(1), b"world")
        finally:
            client.close()


class ParserTests(unittest.TestCase):
    def test_new_commands_and_aliases(self):
        self.assertEqual(parse("status"), StatusCommand())
        self.assertEqual(parse("r 2"), ContextCommand(2, False))
        self.assertEqual(parse("rx 0x3"), ContextCommand(3, True))
        self.assertEqual(parse("stop"), ExecutionCommand("stop"))
        self.assertEqual(parse("bp ffff800000000000"), BreakpointCommand("add", 0xffff800000000000))
        self.assertEqual(parse("bc 7"), BreakpointCommand("remove", 7))
        for text in ("bp", "be", "r 1 2", "status now", "bp -1"):
            with self.subTest(text=text), self.assertRaises(CommandError):
                parse(text)

    def test_memory_grammar_is_bounded_for_v1(self):
        self.assertEqual(parse("rp/b952 1000").count, 952)
        with self.assertRaises(CommandError):
            parse("rp/b953 1000")


class SessionTests(unittest.TestCase):
    def test_negotiation_correlation_and_status(self):
        def handler(transport, data):
            packet = codec.decode(data)
            if packet.message_type == codec.HELLO:
                transport.incoming.append(_hello_response(data))
            elif packet.message_type == codec.QUERY_STATUS:
                transport.incoming.append(codec.packet(
                    codec.QUERY_STATUS, codec.FLAG_RESPONSE,
                    codec.encode_status(codec.Status(codec.TARGET_STOPPED, 0, 1, 1, 0,
                                                     codec.CAP_CONTEXT, 0, 0)),
                    session_id=packet.session_id, sequence=2, request_id=packet.request_id))
        transport = FakeTransport(handler)
        session = Session(transport)
        self.assertEqual(session.connect(1)[0].kind, "connection")
        result = session.execute(StatusCommand(), 1)
        self.assertEqual(result[0].kind, "status_result")
        self.assertEqual(result[0].data["target_state"], codec.TARGET_STOPPED)

    def test_async_stop_and_memory_reply(self):
        context = codec.Amd64Context(1, (0,) * 16, 0x1000, 0x202, 0, 0, 0, 8, 16)

        def handler(transport, data):
            packet = codec.decode(data)
            if packet.message_type == codec.HELLO:
                transport.incoming.append(_hello_response(data))
                transport.incoming.append(codec.packet(
                    codec.STOP, codec.FLAG_EVENT,
                    codec.encode_stop(codec.StopEvent(codec.STOP_REASON_MANUAL,
                                                       codec.STOP_FLAG_RESUMABLE, 1, 0, 1,
                                                       0x1000, 0, context)),
                    session_id=0x1234, sequence=2))
            elif packet.message_type == codec.READ_PHYSICAL:
                transport.incoming.append(codec.packet(
                    packet.message_type, codec.FLAG_RESPONSE,
                    struct.pack("<QIIII", 0x1000, 1, 2, 2, 0) + b"AB",
                    session_id=packet.session_id, sequence=3, request_id=packet.request_id))

        transport = FakeTransport(handler)
        session = Session(transport)
        self.assertEqual(session.connect(1)[0].kind, "connection")
        self.assertEqual(session.wait_for_stop(1)[0].kind, "stop")
        result = session.execute(MemoryCommand(True, False, 0x1000, 1, 2), 1)
        self.assertEqual(result[0].data["values"], [65, 66])

    def test_wrong_request_and_session_are_rejected_and_pending_clears(self):
        def handler(transport, data):
            packet = codec.decode(data)
            if packet.message_type == codec.HELLO:
                transport.incoming.append(_hello_response(data))
            else:
                transport.incoming.append(codec.packet(packet.message_type, codec.FLAG_RESPONSE,
                                                        b"", session_id=packet.session_id,
                                                        sequence=2, request_id=packet.request_id + 1))
        transport = FakeTransport(handler)
        session = Session(transport)
        session.connect(1)
        with self.assertRaises(SessionError):
            session.execute(StatusCommand(), 1)
        self.assertIsNone(session.pending)


class OutputTests(unittest.TestCase):
    def test_json_schema_2_is_deterministic(self):
        stream = io.StringIO()
        renderer = Renderer("jsonl", "never", stream)
        renderer.emit(event("diagnostic", level="info", code="sample", message="first"))
        value = json.loads(stream.getvalue())
        self.assertEqual(value["schema"], 2)
        self.assertEqual(value["seq"], 0)
        self.assertEqual(value["event"], "diagnostic")

    def test_headless_import_stays_lazy(self):
        old = {name: sys.modules.get(name) for name in ("curses", "capstone", "serial")}
        for name in old:
            sys.modules[name] = None
        try:
            sys.modules.pop("src.debugger.headless", None)
            importlib.import_module("src.debugger.headless")
        finally:
            for name, module in old.items():
                if module is None:
                    sys.modules.pop(name, None)
                else:
                    sys.modules[name] = module


if __name__ == "__main__":
    unittest.main()
