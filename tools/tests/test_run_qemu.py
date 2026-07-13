# SPDX-FileCopyrightText: (C) 2026 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock


MODULE_PATH = Path(__file__).parents[1] / "run-qemu.py"
SPEC = importlib.util.spec_from_file_location("palladium_run_qemu", MODULE_PATH)
assert SPEC and SPEC.loader
RUN_QEMU = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RUN_QEMU)


class MarkerTests(unittest.TestCase):
    def test_normalizes_ansi_and_newlines(self):
        value = "\x1b[31mfirst\x1b[0m\r\nsecond\rthird"
        self.assertEqual(RUN_QEMU.normalize_output(value), "first\nsecond\nthird")

    def test_accepts_ordered_success_markers(self):
        output = """
loading up \\EFI\\PALLADIUM\\KERNEL.EXE
loading up \\EFI\\PALLADIUM\\ACPI.SYS
* info: palladium kernel for amd64, git commit 123abcd release build
* info: managing 256 MiB of memory
* info: 1 processor online
* info: enabled ACPI
"""
        tracker = RUN_QEMU.MarkerTracker()
        tracker.scan(output, 1.0)
        self.assertTrue(tracker.complete)
        self.assertEqual([item["name"] for item in tracker.matches], [item[0] for item in RUN_QEMU.MARKERS])

    def test_rejects_out_of_order_success_markers(self):
        output = """
loading up \\EFI\\PALLADIUM\\KERNEL.EXE
* info: palladium kernel for amd64, git commit 123abcd release build
loading up \\EFI\\PALLADIUM\\ACPI.SYS
* info: managing 256 MiB of memory
* info: 1 processor online
* info: enabled ACPI
"""
        tracker = RUN_QEMU.MarkerTracker()
        tracker.scan(output, 1.0)
        self.assertFalse(tracker.complete)

    def test_detects_loader_and_kernel_failures(self):
        loader = RUN_QEMU.find_failure("The boot process cannot continue.\n")
        kernel = RUN_QEMU.find_failure("*** STOP: KERNEL_INITIALIZATION_FAILURE\n")
        self.assertEqual(loader["name"], "loader-failure")
        self.assertEqual(kernel["name"], "kernel-fatal")

    def test_accepts_ordered_self_test_markers_and_records(self):
        arguments = SimpleNamespace(mode="selftest", smp=4)
        output = """
loading up \\EFI\\PALLADIUM\\KERNEL.EXE
loading up \\EFI\\PALLADIUM\\ACPI.SYS
palladium kernel for amd64, git commit 123abcd debug build
managing 256 MiB of memory
4 processors online
enabled ACPI
M1TEST START suite=portable-rt seed=c001d00d cpus=4
M1TEST PASS suite=portable-rt cases=3
M1TEST COMPLETE cases=3
"""
        tracker = RUN_QEMU.MarkerTracker(RUN_QEMU.markers_for(arguments))
        tracker.scan(output, 1.0)
        self.assertTrue(tracker.complete)
        self.assertEqual(
            RUN_QEMU.parse_self_test_records(output),
            [
                {
                    "action": "start",
                    "suite": "portable-rt",
                    "seed": "c001d00d",
                    "cpus": 4,
                },
                {"action": "pass", "suite": "portable-rt", "cases": 3},
                {"action": "complete", "cases": 3},
            ],
        )

    def test_detects_self_test_failure(self):
        failure = RUN_QEMU.find_failure("M1TEST FAIL suite=mm case=3 code=9\n")
        self.assertEqual(failure["name"], "self-test-failure")

    def test_aggregates_loader_and_serial_debugger_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "loader.log").write_text(
                "loading up \\EFI\\PALLADIUM\\KERNEL.EXE\n"
                "loading up \\EFI\\PALLADIUM\\ACPI.SYS\n")
            (root / "debugger.jsonl").write_text(json.dumps({
                "schema": 2,
                "seq": 0,
                "event": "target_output",
                "data": {"text": "palladium kernel for amd64, git commit abc release build\n"},
            }) + "\n" + json.dumps({
                "schema": 2,
                "seq": 1,
                "event": "target_output",
                "data": {"text": "managing 256 MiB of memory\n1 processor online\n"
                                  "enabled ACPI\n"},
            }) + "\n")
            aggregate = RUN_QEMU.aggregate_output(root / "loader.log", root / "debugger.jsonl")
        tracker = RUN_QEMU.MarkerTracker()
        tracker.scan(aggregate, 1.0)
        self.assertTrue(tracker.complete)

    def test_serial_debugger_command_uses_frozen_headless_form(self):
        command = RUN_QEMU.debugger_command(Path("/tmp/run-001/kd.sock"))
        self.assertEqual(
            command,
            [
                "python3", "-m", "src.debugger", "serial", "unix:/tmp/run-001/kd.sock",
                "--headless", "--format", "jsonl",
            ],
        )

    def test_serial_debugger_command_accepts_one_script_or_commands(self):
        scripted = RUN_QEMU.debugger_command(Path("/tmp/kd.sock"), script="commands.kd")
        self.assertEqual(scripted[-2:], ["--script", "commands.kd"])
        commands = RUN_QEMU.debugger_command(
            Path("/tmp/kd.sock"), commands=["status", "continue", "detach"])
        self.assertEqual(commands[-6:], ["--command", "status", "--command", "continue",
                                         "--command", "detach"])


class FakeProcess:
    def __init__(self, return_code=None):
        self.returncode = return_code

    def poll(self):
        return self.returncode

    def terminate(self):
        self.returncode = -15

    def kill(self):
        self.returncode = -9

    def wait(self, timeout=None):
        if self.returncode is None:
            self.returncode = 0
        return self.returncode


class BrokenQmp:
    def sendall(self, _data):
        raise OSError("simulated QMP failure")

    def close(self):
        pass


class RunnerTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        for name in ("image.iso", "code.fd", "vars.fd"):
            (self.root / name).write_bytes(name.encode())

    def tearDown(self):
        self.temporary.cleanup()

    def arguments(self, output: str, timeout=0.001):
        return SimpleNamespace(
            qemu="qemu-system-x86_64",
            mode="smoke",
            image=self.root / "image.iso",
            ovmf_code=self.root / "code.fd",
            ovmf_vars=self.root / "vars.fd",
            output_dir=self.root / output,
            timeout=timeout,
            qemu_args=[],
        )

    def test_fixed_profile_disables_network_and_uses_per_run_paths(self):
        args = self.arguments("unused")
        first = RUN_QEMU.build_command(
            args, self.root / "run-001" / "vars.fd",
            self.root / "run-001" / "serial.log", self.root / "run-001" / "qmp.sock")
        second = RUN_QEMU.build_command(
            args, self.root / "run-002" / "vars.fd",
            self.root / "run-002" / "serial.log", self.root / "run-002" / "qmp.sock")
        self.assertIn("pc-q35-8.2", first)
        self.assertEqual(first[first.index("-nic") + 1], "none")
        self.assertIn("-chardev", first)
        self.assertIn("socket,id=kd", first[first.index("-chardev") + 1])
        self.assertEqual(first[first.index("-serial", first.index("-chardev")) + 1], "chardev:kd")
        self.assertNotEqual(first, second)

    def test_selftest_profile_uses_requested_processor_count(self):
        args = self.arguments("unused")
        args.mode = "selftest"
        args.smp = 4
        command = RUN_QEMU.build_command(
            args,
            self.root / "run-001" / "vars.fd",
            self.root / "run-001" / "serial.log",
            self.root / "run-001" / "qmp.sock",
        )
        self.assertEqual(command[command.index("-smp") + 1], "4")
        self.assertEqual(command[command.index("-nic") + 1], "none")

    def test_debugger_profile_is_fixed_one_cpu_and_accepts_serial_channel(self):
        args = self.arguments("unused")
        args.mode = "debugger"
        command = RUN_QEMU.build_command(
            args,
            self.root / "run-001" / "vars.fd",
            self.root / "run-001" / "loader.log",
            self.root / "run-001" / "qmp.sock",
        )
        self.assertEqual(command[command.index("-smp") + 1], "1")
        self.assertEqual(command[command.index("-serial") + 1],
                         "file:" + str(self.root / "run-001" / "loader.log"))
        self.assertIn("chardev:kd", command)

    def test_early_exit_fails_and_preserves_fresh_variable_copy(self):
        args = self.arguments("early")
        args.output_dir.mkdir()
        with mock.patch.object(RUN_QEMU.subprocess, "Popen", return_value=FakeProcess(7)), \
                mock.patch.object(RUN_QEMU, "connect_qmp", return_value=None):
            result = RUN_QEMU.run_once(args, 1, "fake qemu")
        self.assertEqual(result["status"], "early-exit")
        self.assertFalse(result["passed"])
        self.assertEqual((args.output_dir / "run-001" / "ovmf-vars.fd").read_bytes(), b"vars.fd")
        self.assertEqual(args.ovmf_vars.read_bytes(), b"vars.fd")

    def test_timeout_fails_and_terminates(self):
        args = self.arguments("timeout")
        args.output_dir.mkdir()
        process = FakeProcess()
        with mock.patch.object(RUN_QEMU.subprocess, "Popen", return_value=process), \
                mock.patch.object(RUN_QEMU, "connect_qmp", return_value=None):
            result = RUN_QEMU.run_once(args, 1, "fake qemu")
        self.assertEqual(result["status"], "timeout")
        self.assertEqual(result["stop_method"], "terminate")

    def test_qmp_failure_uses_termination_fallback(self):
        args = self.arguments("qmp", timeout=1)
        args.output_dir.mkdir()
        process = FakeProcess()

        def launch(command, **_kwargs):
            if "-serial" not in command:
                return FakeProcess()
            serial = Path(command[command.index("-serial") + 1].removeprefix("file:"))
            serial.write_text(
                "loading up \\EFI\\PALLADIUM\\KERNEL.EXE\n"
                "loading up \\EFI\\PALLADIUM\\ACPI.SYS\n"
                "palladium kernel for amd64, git commit abc release build\n"
                "managing 256 MiB of memory\n1 processor online\nenabled ACPI\n")
            return process

        with mock.patch.object(RUN_QEMU.subprocess, "Popen", side_effect=launch), \
                mock.patch.object(RUN_QEMU, "connect_qmp", return_value=BrokenQmp()):
            result = RUN_QEMU.run_once(args, 1, "fake qemu")
        self.assertTrue(result["passed"])
        self.assertEqual(result["stop_method"], "terminate")


if __name__ == "__main__":
    unittest.main()
