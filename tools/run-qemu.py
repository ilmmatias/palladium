#!/usr/bin/env python3
# SPDX-FileCopyrightText: (C) 2026 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

"""Run Palladium under QEMU in reproducible smoke, self-test, or interactive modes."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import socket
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

ANSI_ESCAPE = re.compile(r"\x1b(?:\[[0-?]*[ -/]*[@-~]|\][^\x07]*(?:\x07|\x1b\\))")
SMOKE_MARKERS = (
    ("loader-kernel", r"loading up\s+[\\/]+EFI[\\/]+PALLADIUM[\\/]+KERNEL\.EXE"),
    ("loader-acpi", r"loading up\s+[\\/]+EFI[\\/]+PALLADIUM[\\/]+ACPI\.SYS"),
    ("kernel-banner", r"palladium kernel for amd64, git commit .* (?:release|debug) build"),
    ("memory-manager", r"managing [0-9]+ MiB of memory"),
    ("processor-online", r"1 processor online"),
    ("acpi-enabled", r"enabled ACPI"),
)
SELF_TEST_MARKERS = (
    ("self-test-start", r"M1TEST START suite=[^\s]+ seed=[0-9a-f]+ cpus=[0-9]+"),
    ("self-test-pass", r"M1TEST PASS suite=[^\s]+ cases=[0-9]+"),
    ("self-test-complete", r"M1TEST COMPLETE cases=[0-9]+"),
)
MARKERS = SMOKE_MARKERS
FAILURES = (
    ("loader-failure", r"The boot process cannot continue\."),
    ("kernel-fatal", r"\*\*\* STOP:"),
    ("self-test-failure", r"M1TEST FAIL suite=[^\s]+ case=[^\s]+ code=[^\s]+"),
)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="mode", required=True)
    for name in ("smoke", "selftest", "debugger", "interactive"):
        command = subparsers.add_parser(name)
        command.add_argument("--image", required=True, type=Path, help="ISO or FAT image")
        command.add_argument("--ovmf-code", required=True, type=Path)
        command.add_argument("--ovmf-vars", required=True, type=Path)
        command.add_argument("--output-dir", required=True, type=Path)
        command.add_argument("--timeout", type=float, default=120.0 if name == "selftest" else 60.0)
        command.add_argument("--repeat", type=int, default=1)
        command.add_argument("--qemu", default="qemu-system-x86_64")
        if name in ("selftest", "debugger"):
            command.add_argument("--smp", type=int, choices=(1, 2, 4), default=1)
        if name == "debugger":
            sources = command.add_mutually_exclusive_group()
            sources.add_argument("--script", metavar="PATH",
                                 help="serial debugger UTF-8 command script")
            sources.add_argument("--command", action="append", default=[], metavar="COMMAND",
                                 help="serial debugger command (repeatable)")
        command.add_argument("qemu_args", nargs=argparse.REMAINDER,
                             help="extra QEMU arguments after --")
    arguments = parser.parse_args()
    if arguments.qemu_args[:1] == ["--"]:
        arguments.qemu_args = arguments.qemu_args[1:]
    if arguments.mode in ("smoke", "selftest", "debugger") and arguments.qemu_args:
        parser.error(f"the fixed {arguments.mode} profile does not accept extra QEMU arguments")
    return arguments


def normalize_output(value: str) -> str:
    return ANSI_ESCAPE.sub("", value).replace("\r\n", "\n").replace("\r", "\n")


def debugger_command(
    socket_path: Path,
    script: str | None = None,
    commands: list[str] | None = None,
) -> list[str]:
    """Return the repository debugger's fixed serial headless invocation."""
    command = [
        "python3",
        "-m",
        "src.debugger",
        "serial",
        f"unix:{socket_path}",
        "--headless",
        "--format",
        "jsonl",
    ]
    if script:
        command += ["--wait-output", "enabled ACPI", "--script", script]
    elif commands:
        command += ["--wait-output", "enabled ACPI"]
        for item in commands:
            command += ["--command", item]
    return command


def debugger_target_output(path: Path) -> str:
    """Extract target output from the debugger's JSONL stream.

    Debugger diagnostics and connection events are retained in the raw JSONL artifact but do not
    participate in boot-marker matching. Schema 1 is accepted for compatibility with an older
    client; the serial client emits schema 2.
    """
    try:
        lines = path.read_text(errors="replace").splitlines()
    except OSError:
        return ""
    output: list[str] = []
    for line in lines:
        try:
            value = json.loads(line)
        except json.JSONDecodeError:
            continue
        if value.get("event") != "target_output":
            continue
        data = value.get("data")
        if isinstance(data, dict) and isinstance(data.get("text"), str):
            output.append(data["text"])
    return normalize_output("".join(output))


def aggregate_output(loader_path: Path, debugger_path: Path | None = None) -> str:
    """Combine loader ConOut (COM1) with target output received by serial KD (COM2)."""
    try:
        loader = normalize_output(loader_path.read_text(errors="replace"))
    except OSError:
        loader = ""
    target = debugger_target_output(debugger_path) if debugger_path is not None else ""
    if loader and target and not loader.endswith("\n"):
        loader += "\n"
    return loader + target


class MarkerTracker:
    def __init__(self, markers: tuple[tuple[str, str], ...] = MARKERS) -> None:
        self.markers = markers
        self.index = 0
        self.offset = 0
        self.matches: list[dict[str, Any]] = []

    def scan(self, text: str, elapsed: float) -> None:
        while self.index < len(self.markers):
            name, pattern = self.markers[self.index]
            match = re.search(pattern, text[self.offset :], re.IGNORECASE)
            if not match:
                break
            self.offset += match.end()
            self.matches.append(
                {"name": name, "pattern": pattern, "elapsed_seconds": round(elapsed, 3)}
            )
            self.index += 1

    @property
    def complete(self) -> bool:
        return self.index == len(self.markers)


def markers_for(args: argparse.Namespace) -> tuple[tuple[str, str], ...]:
    markers = list(SMOKE_MARKERS)
    if args.mode in ("selftest", "debugger"):
        processor_count = getattr(args, "smp", 1)
        markers[4] = (
            "processor-online",
            rf"{processor_count} processor{'s' if processor_count != 1 else ''} online",
        )
    if args.mode == "selftest":
        markers.extend(SELF_TEST_MARKERS)
    return tuple(markers)


def parse_self_test_records(text: str) -> list[dict[str, Any]]:
    records = []
    pattern = re.compile(r"M1TEST (START|PASS|FAIL|COMPLETE)(?:\s+([^\r\n]*))?")
    for match in pattern.finditer(text):
        record: dict[str, Any] = {"action": match.group(1).lower()}
        for name, value in re.findall(r"([a-z]+)=([^\s]+)", match.group(2) or ""):
            if name in ("cases", "case", "code", "cpus"):
                record[name] = int(value, 10)
            else:
                record[name] = value
        records.append(record)
    return records


def find_failure(text: str) -> dict[str, str] | None:
    for name, pattern in FAILURES:
        if re.search(pattern, text, re.IGNORECASE):
            return {"name": name, "pattern": pattern}
    return None


def file_hash(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def qemu_version(binary: str) -> str:
    try:
        result = subprocess.run(
            [binary, "--version"], check=False, capture_output=True, text=True, timeout=5
        )
        return (result.stdout or result.stderr).splitlines()[0]
    except (OSError, subprocess.TimeoutExpired):
        return "unavailable"


def connect_qmp(path: Path, process: subprocess.Popen[bytes], log_path: Path) -> socket.socket | None:
    deadline = time.monotonic() + 2
    while process.poll() is None and time.monotonic() < deadline:
        qmp = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            qmp.settimeout(0.2)
            qmp.connect(str(path))
            greeting = qmp.recv(65536)
            with log_path.open("ab") as log:
                log.write(greeting)
            qmp.sendall(b'{"execute":"qmp_capabilities"}\r\n')
            try:
                response = qmp.recv(65536)
                with log_path.open("ab") as log:
                    log.write(response)
            except TimeoutError:
                pass
            return qmp
        except OSError:
            qmp.close()
            time.sleep(0.02)
    return None


def stop_qemu(process: subprocess.Popen[bytes], qmp: socket.socket | None) -> str:
    if process.poll() is not None:
        return "already-exited"
    if qmp is not None:
        try:
            qmp.sendall(b'{"execute":"quit"}\r\n')
            process.wait(timeout=5)
            return "qmp-quit"
        except (OSError, subprocess.TimeoutExpired):
            pass
    process.terminate()
    try:
        process.wait(timeout=5)
        return "terminate"
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()
        return "kill"


def capture_screenshot(qmp: socket.socket | None, path: Path, log_path: Path) -> bool:
    """Ask QMP for a framebuffer dump while a failed guest is still alive."""
    if qmp is None:
        return False
    request = json.dumps({
        "execute": "screendump",
        "arguments": {"filename": str(path)},
    }, separators=(",", ":")).encode() + b"\r\n"
    try:
        qmp.sendall(request)
        qmp.settimeout(1.0)
        response = qmp.recv(65536)
        with log_path.open("ab") as log:
            log.write(response)
        return path.exists()
    except (AttributeError, OSError, TimeoutError):
        return False


def stop_debugger(process: subprocess.Popen[bytes] | None) -> str:
    if process is None or process.poll() is not None:
        return "already-exited" if process is not None else "not-started"
    process.terminate()
    try:
        process.wait(timeout=5)
        return "terminate"
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()
        return "kill"


def build_command(
    args: argparse.Namespace,
    vars_copy: Path,
    loader_serial_path: Path,
    qmp_path: Path,
    kd_socket_path: Path | None = None,
) -> list[str]:
    command = [args.qemu]
    if args.mode in ("smoke", "selftest", "debugger"):
        processor_count = 1 if args.mode == "smoke" else getattr(args, "smp", 1)
        command += [
            "-accel",
            "tcg",
            "-machine",
            "pc-q35-8.2",
            "-cpu",
            "qemu64",
            "-m",
            "256M",
            "-smp",
            str(processor_count),
            "-display",
            "none",
            "-nic",
            "none",
        ]
    elif os.path.exists("/dev/kvm") and os.access("/dev/kvm", os.R_OK | os.W_OK):
        command += [
            "-accel",
            "kvm",
            "-machine",
            "q35",
            "-cpu",
            "host,+invtsc",
            "-m",
            "256M",
            "-smp",
            str(os.cpu_count() or 1),
        ]
    else:
        command += [
            "-accel",
            "tcg",
            "-machine",
            "pc-q35-8.2",
            "-cpu",
            "qemu64",
            "-m",
            "256M",
            "-smp",
            "1",
        ]
    command += [
        "-drive",
        f"if=pflash,format=raw,unit=0,file={args.ovmf_code},readonly=on",
        "-drive",
        f"if=pflash,format=raw,unit=1,file={vars_copy}",
        "-serial",
        f"file:{loader_serial_path}",
    ]
    if args.mode in ("smoke", "selftest", "debugger"):
        if kd_socket_path is None:
            kd_socket_path = loader_serial_path.with_name("kd.sock")
        command += [
            "-chardev",
            f"socket,id=kd,path={kd_socket_path},server=on,wait=off,"
            f"logfile={kd_socket_path.with_name('kd-stream.log')}",
            "-serial",
            "chardev:kd",
        ]
    command += [
        "-qmp",
        f"unix:{qmp_path},server=on,wait=off",
        "-no-reboot",
        "-no-shutdown",
    ]
    command += (
        ["-cdrom", str(args.image)]
        if args.image.suffix.lower() == ".iso"
        else ["-drive", f"format=raw,file={args.image}"]
    )
    return command + args.qemu_args


def run_once(args: argparse.Namespace, run_number: int, version: str) -> dict[str, Any]:
    run_dir = args.output_dir / f"run-{run_number:03d}"
    run_dir.mkdir()
    vars_copy = run_dir / "ovmf-vars.fd"
    shutil.copyfile(args.ovmf_vars, vars_copy)
    loader_serial_path = run_dir / "loader.log"
    serial_path = run_dir / "serial.log"
    debugger_log_path = run_dir / "debugger.jsonl"
    debugger_error_path = run_dir / "debugger.stderr.log"
    qmp_log_path = run_dir / "qmp.log"
    qmp_socket_path = run_dir / "qmp.sock"
    kd_socket_path = run_dir / "kd.sock"
    screenshot_path = run_dir / "screenshot.ppm"
    result_path = run_dir / "result.json"
    loader_serial_path.touch()
    serial_path.touch()
    debugger_log_path.touch()
    debugger_error_path.touch()
    qmp_log_path.touch()
    command = build_command(args, vars_copy, loader_serial_path, qmp_socket_path, kd_socket_path)
    debug_command = debugger_command(
        kd_socket_path,
        (str(Path(args.script).resolve()) if getattr(args, "script", None) not in (None, "-")
         else getattr(args, "script", None)),
        getattr(args, "command", None),
    )
    started = time.monotonic()
    markers = markers_for(args)
    tracker = MarkerTracker(markers)
    failure = None
    status = "launch-error"
    stop_method = "not-started"
    process: subprocess.Popen[bytes] | None = None
    debugger_process: subprocess.Popen[bytes] | None = None
    qmp = None
    screenshot_captured = False

    try:
        process = subprocess.Popen(command)
        qmp = connect_qmp(qmp_socket_path, process, qmp_log_path)
        debugger_ready = True
        if args.mode in ("smoke", "selftest", "debugger"):
            # OVMF exposes the UARTs as firmware consoles. Sending binary KD frames before the
            # loader is running can therefore be interpreted as firmware keyboard input. Wait for
            # both loader records on COM1 before connecting the COM2 debugger client.
            loader_deadline = started + args.timeout
            debugger_ready = False
            while time.monotonic() < loader_deadline:
                loader_output = normalize_output(loader_serial_path.read_text(errors="replace"))
                if all(re.search(pattern, loader_output, re.IGNORECASE)
                       for _, pattern in SMOKE_MARKERS[:2]):
                    debugger_ready = True
                    break
                if process.poll() is not None:
                    status = "early-exit"
                    break
                time.sleep(0.05)
            if not debugger_ready and status != "early-exit":
                status = "timeout"
            if debugger_ready:
                with debugger_log_path.open("ab") as debugger_log, \
                        debugger_error_path.open("ab") as debugger_error:
                    debugger_process = subprocess.Popen(
                        debug_command,
                        cwd=Path(__file__).resolve().parents[1],
                        stdin=(None if getattr(args, "script", None) == "-" else subprocess.DEVNULL),
                        stdout=debugger_log,
                        stderr=debugger_error,
                    )
        if args.mode == "interactive":
            process.wait()
            status = "exited"
        elif debugger_ready:
            deadline = time.monotonic() + args.timeout
            while time.monotonic() < deadline:
                output = aggregate_output(loader_serial_path, debugger_log_path)
                serial_path.write_text(output)
                tracker.scan(output, time.monotonic() - started)
                failure = find_failure(output)
                if failure:
                    status = "failed-marker"
                    break
                if tracker.complete:
                    if args.mode != "debugger":
                        status = "passed"
                        break
                    if debugger_process is not None and debugger_process.poll() is not None:
                        status = "passed" if debugger_process.returncode == 0 else "debugger-exit"
                        if debugger_process.returncode:
                            failure = {
                                "name": "debugger-exit",
                                "message":
                                    f"serial debugger exited with code {debugger_process.returncode}",
                            }
                        break
                if process.poll() is not None:
                    status = "early-exit"
                    break
                if debugger_process is not None and debugger_process.poll() is not None:
                    status = "debugger-exit"
                    failure = {
                        "name": "debugger-exit",
                        "message": f"serial debugger exited with code {debugger_process.returncode}",
                    }
                    break
                time.sleep(0.1)
            else:
                status = "timeout"
    except OSError as error:
        status = "launch-error"
        failure = {"name": "launch-error", "message": str(error)}
    finally:
        serial_path.write_text(aggregate_output(loader_serial_path, debugger_log_path))
        if status != "passed":
            screenshot_captured = capture_screenshot(qmp, screenshot_path, qmp_log_path)
        if process is not None:
            stop_method = stop_qemu(process, qmp)
        debugger_stop_method = stop_debugger(debugger_process)
        if qmp is not None:
            qmp.close()

    aggregate = normalize_output(serial_path.read_text(errors="replace"))

    result = {
        "status": status,
        "passed": status == "passed"
        if args.mode in ("smoke", "selftest", "debugger")
        else bool(process and process.returncode == 0),
        "return_code": process.returncode if process is not None else None,
        "stop_method": stop_method,
        "duration_seconds": round(time.monotonic() - started, 3),
        "matched_markers": tracker.matches,
        "expected_markers": [{"name": name, "pattern": pattern} for name, pattern in markers],
        "self_tests": parse_self_test_records(aggregate),
        "failure": failure,
        "command": command,
        "debugger_command": debug_command,
        "debugger_stop_method": debugger_stop_method,
        "loader_serial_log": str(loader_serial_path),
        "debugger_log": str(debugger_log_path),
        "debugger_stderr_log": str(debugger_error_path),
        "screenshot": str(screenshot_path) if screenshot_captured else None,
        "qemu_version": version,
        "inputs": {
            "image": {"path": str(args.image), "sha256": file_hash(args.image)},
            "ovmf_code": {"path": str(args.ovmf_code), "sha256": file_hash(args.ovmf_code)},
            "ovmf_vars_template": {
                "path": str(args.ovmf_vars),
                "sha256": file_hash(args.ovmf_vars),
            },
        },
    }
    result_path.write_text(json.dumps(result, indent=2) + "\n")
    return result


def main() -> int:
    args = parse_arguments()
    if args.repeat < 1 or args.timeout <= 0:
        raise SystemExit("--repeat and --timeout must be positive")
    for path in (args.image, args.ovmf_code, args.ovmf_vars):
        if not path.is_file():
            raise SystemExit(f"file not found: {path}")
    if args.output_dir.exists():
        if not args.output_dir.is_dir():
            raise SystemExit(f"output path is not a directory: {args.output_dir}")
        if any(args.output_dir.iterdir()):
            raise SystemExit(f"refusing to use nonempty output directory: {args.output_dir}")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    version = qemu_version(args.qemu)
    results = [run_once(args, number, version) for number in range(1, args.repeat + 1)]
    summary = {"passed": all(item["passed"] for item in results), "runs": results}
    (args.output_dir / "result.json").write_text(json.dumps(summary, indent=2) + "\n")
    return 0 if summary["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
