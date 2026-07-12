# M0 reproducible evidence plane

M0 supplies two deliberately independent evidence lanes. The boot lane is proprietary-free and
uses a boot-configured, polling amd64 PC16550 diagnostic sink. The host-debugger lane tests the
temporary v0 codec, peer-validating UDP transport, session state, command service and both frontend
rendering paths without requiring a KDNET extension module.

## Boot evidence lane

`tools/smoke.sh` is the one-command entry point after a Palladium build exists. It invokes
`tools/build-image.sh` with ACPI and serial diagnostics enabled, then invokes the fixed `smoke`
subcommand of `tools/run-qemu.py`. Each QEMU run receives its own copied OVMF variable store,
serial/QMP logs and machine-readable result. The smoke profile is `pc-q35-8.2`, TCG, `qemu64`, 256
MiB, one processor, no display and no NIC. It observes the loader's kernel and ACPI loads followed
by the kernel banner, MM message, one-processor message and ACPI-enable message in order.

The ordinary image path remains framebuffer-only unless `DiagnosticDevice=PC16550` is explicitly
configured. Loader block v7 is an intentionally incompatible internal handoff: loader and kernel
both assert the numeric packed layout, and the first kernel entry validates magic, exact version
and exact block size before using any versioned records. The new UART mechanism remains private to
the amd64 HAL and is not a debugger transport or a kernel export.

## Debugger evidence lane

`python3 -m src.debugger IP PORT` remains the curses TUI. Adding `--headless` selects a renderer that
does not import curses; Capstone is imported only for a disassembly request. Both frontends consume
the same pure command parser and typed session events. Headless scripts can emit stable text or
schema-1 JSONL and use deterministic exit codes for success, runtime failure, usage failure and
timeouts.

The v0 codec is temporary and intentionally strict. It validates exact fixed packet sizes, memory
length/count relationships, the target payload ceiling, UTF-8 print data, the connected peer and
the one-pending-request rule. Invalid UTF-8 print data produces a protocol diagnostic and is
ignored; malformed or unexpected acknowledgements cannot satisfy a request. V0 does not claim
heartbeat, reconnect, replay defense or post-connect recovery.

Run the host evidence with:

```sh
python3 -m unittest discover -s src/debugger/tests
python3 -m unittest discover -s tools/tests
```

The localhost fake target exercises only the Python client. It does not traverse Palladium's raw
Ethernet/IPv4/UDP target stack or any proprietary KD extension module.

## Qualification record and limitations

On 2026-07-12, a clean amd64 Release configuration built all 345 targets with Clang/LLVM 22.1.8.
The generated image then passed 20 consecutive runs of the exact TCG smoke profile under QEMU
10.2.3. Every run matched all six markers in order and exited through QMP; observed durations were
1.025 to 1.053 seconds. The debugger suite passed 18 tests and the runner suite passed eight.

This record qualifies the local smoke gate, not the hosted runner. The workflow must still be
observed on the contained branch before relying on it as a protected required check. Physical
hardware, SMP boots, the TUI under an interactive terminal and the kernel KD extension path were
not exercised by this qualification.

The fixed 256-MiB QEMU profile currently prints `managing 12801 MiB of memory`. M0 treats the marker
as evidence that MM initialization was reached, not as proof that the accounting value is correct;
the discrepancy is a high-priority input to the M1 memory audit.

The existing clang-tidy baseline remains the 685-diagnostic LLVM 22 result recorded in
`docs/roadmap/CoreCorrectnessAndVerification.md`. M0 neither reran nor gated on clang-tidy, and
warning classification/fixes remain M1 work.
