# M0 reproducible evidence plane

M0 supplied two deliberately independent evidence lanes. The M2 migration keeps the proprietary-free
boot lane but replaces its one-way diagnostic sink with the Palladium serial debugger on COM2. The
host-debugger lane now exercises the serial client/session path in addition to the temporary v0
codec, peer-validating UDP transport, command service and frontend rendering paths.

## Boot evidence lane

`tools/smoke.sh` is the one-command entry point after a Palladium build exists. It invokes
`tools/build-image.sh --debug-serial`, then invokes the fixed `smoke` subcommand of
`tools/run-qemu.py`. Each QEMU run receives its own copied OVMF variable store, COM1 loader log,
COM2 debugger JSONL/QMP logs and machine-readable result. The smoke profile is `pc-q35-8.2`, TCG,
`qemu64`, 256 MiB, one processor, no display and no NIC. It observes the loader's kernel and ACPI
loads followed by target-output kernel banner, MM message, one-processor message and ACPI-enable
message in order.

The ordinary image path remains framebuffer-only and debugger-disabled unless `DebugTransport` is
explicitly configured. Loader ConOut remains on COM1 (`0x3F8`); serial KD uses COM2 (`0x2F8`) at
115200 baud. Loader block v8 is an intentionally incompatible internal handoff: loader and kernel
both assert the numeric packed layout, and the first kernel entry validates magic, exact version
and exact block size before using any versioned records. The serial transport is owned by KD and is
not a diagnostic-write shortcut.

## Debugger evidence lane

`python3 -m src.debugger IP PORT` remains the curses TUI. The serial headless entry point is
`python3 -m src.debugger serial unix:PATH --headless --format jsonl`; it does not import curses.
Both frontends consume the same pure command parser and typed session events. Headless scripts can
emit stable text or schema-2 JSONL and use deterministic exit codes for success, runtime failure,
usage failure and timeouts.

The v0 codec is temporary and intentionally strict. It validates exact fixed packet sizes, memory
length/count relationships, the target payload ceiling, UTF-8 print data, the connected peer and
the one-pending-request rule. Invalid UTF-8 print data produces a protocol diagnostic and is
ignored; malformed or unexpected acknowledgements cannot satisfy a request. The serial headless
client emits schema-2 JSONL target-output events and supports reconnect/loss policy tests; UDP v0
does not claim heartbeat, replay defense or post-connect recovery.

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
