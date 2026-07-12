#!/usr/bin/env python3
# SPDX-FileCopyrightText: (C) 2026 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

"""Run Palladium under QEMU in reproducible smoke or interactive modes."""

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
MARKERS = (
    ("loader-kernel", r"loading up\s+[\\/]+EFI[\\/]+PALLADIUM[\\/]+KERNEL\.EXE"),
    ("loader-acpi", r"loading up\s+[\\/]+EFI[\\/]+PALLADIUM[\\/]+ACPI\.SYS"),
    ("kernel-banner", r"palladium kernel for amd64, git commit .* (?:release|debug) build"),
    ("memory-manager", r"managing [0-9]+ MiB of memory"),
    ("processor-online", r"1 processor online"),
    ("acpi-enabled", r"enabled ACPI"),
)
FAILURES = (
    ("loader-failure", r"The boot process cannot continue\."),
    ("kernel-fatal", r"\*\*\* STOP:"),
)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="mode", required=True)
    for name in ("smoke", "interactive"):
        command = subparsers.add_parser(name)
        command.add_argument("--image", required=True, type=Path, help="ISO or FAT image")
        command.add_argument("--ovmf-code", required=True, type=Path)
        command.add_argument("--ovmf-vars", required=True, type=Path)
        command.add_argument("--output-dir", required=True, type=Path)
        command.add_argument("--timeout", type=float, default=60.0)
        command.add_argument("--repeat", type=int, default=1)
        command.add_argument("--qemu", default="qemu-system-x86_64")
        command.add_argument("qemu_args", nargs=argparse.REMAINDER,
                             help="extra QEMU arguments after --")
    arguments = parser.parse_args()
    if arguments.qemu_args[:1] == ["--"]:
        arguments.qemu_args = arguments.qemu_args[1:]
    if arguments.mode == "smoke" and arguments.qemu_args:
        parser.error("the fixed smoke profile does not accept extra QEMU arguments")
    return arguments


def normalize_output(value: str) -> str:
    return ANSI_ESCAPE.sub("", value).replace("\r\n", "\n").replace("\r", "\n")


class MarkerTracker:
    def __init__(self) -> None:
        self.index = 0
        self.offset = 0
        self.matches: list[dict[str, Any]] = []

    def scan(self, text: str, elapsed: float) -> None:
        while self.index < len(MARKERS):
            name, pattern = MARKERS[self.index]
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
        return self.index == len(MARKERS)


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


def build_command(args: argparse.Namespace, vars_copy: Path, serial_path: Path, qmp_path: Path) -> list[str]:
    command = [args.qemu]
    if args.mode == "smoke":
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
        f"file:{serial_path}",
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
    serial_path = run_dir / "serial.log"
    qmp_log_path = run_dir / "qmp.log"
    qmp_socket_path = run_dir / "qmp.sock"
    result_path = run_dir / "result.json"
    serial_path.touch()
    qmp_log_path.touch()
    command = build_command(args, vars_copy, serial_path, qmp_socket_path)
    started = time.monotonic()
    tracker = MarkerTracker()
    failure = None
    status = "launch-error"
    stop_method = "not-started"
    process: subprocess.Popen[bytes] | None = None
    qmp = None

    try:
        process = subprocess.Popen(command)
        qmp = connect_qmp(qmp_socket_path, process, qmp_log_path)
        if args.mode == "interactive":
            process.wait()
            status = "exited"
        else:
            deadline = time.monotonic() + args.timeout
            while time.monotonic() < deadline:
                output = normalize_output(serial_path.read_text(errors="replace"))
                tracker.scan(output, time.monotonic() - started)
                failure = find_failure(output)
                if failure:
                    status = "failed-marker"
                    break
                if tracker.complete:
                    status = "passed"
                    break
                if process.poll() is not None:
                    status = "early-exit"
                    break
                time.sleep(0.1)
            else:
                status = "timeout"
    except OSError as error:
        status = "launch-error"
        failure = {"name": "launch-error", "message": str(error)}
    finally:
        if process is not None:
            stop_method = stop_qemu(process, qmp)
        if qmp is not None:
            qmp.close()

    result = {
        "status": status,
        "passed": status == "passed" if args.mode == "smoke" else bool(process and process.returncode == 0),
        "return_code": process.returncode if process is not None else None,
        "stop_method": stop_method,
        "duration_seconds": round(time.monotonic() - started, 3),
        "matched_markers": tracker.matches,
        "expected_markers": [{"name": name, "pattern": pattern} for name, pattern in MARKERS],
        "failure": failure,
        "command": command,
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
