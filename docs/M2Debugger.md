# M2 serial debugger evidence

The fixed `smoke` and `selftest` QEMU profiles build images with `--debug-serial`. The image keeps
UEFI loader ConOut on COM1 (`0x3F8`) and configures Palladium's PC16550 debugger on COM2
(`0x2F8`, 115200 baud). A Unix chardev socket connects COM2 to the headless client:

```sh
python3 -m src.debugger serial unix:/path/to/kd.sock --headless --format jsonl
```

`tools/run-qemu.py` launches that client automatically for fixed profiles. Each run retains the
legacy `serial.log` aggregate (loader text followed by `target_output` events), plus `loader.log`,
`debugger.jsonl`, `debugger.stderr.log`, `qmp.log`, and a copied `ovmf-vars.fd`. A failed run asks
QMP for `screenshot.ppm` before shutdown when the display backend permits it. Per-run and summary
`result.json` files retain marker matches, self-test records, command lines, input hashes, and
shutdown methods.

The `debugger` subcommand is a deterministic, non-gating variant of the same profile. It accepts
one UTF-8 `--script` or repeatable `--command` options for status/context, breakpoint, continue,
step, and detach scenarios (`r`/`rx` query context), and accepts `--smp 1`, `2`, or `4`. Scripted
commands begin only after the ACPI boot marker so firmware cannot interpret framed COM2 traffic as
console input.

The serial disconnect policy defaults to `STOP` with a 5000 ms timeout. The client refreshes target
liveness once per second. STOP retains contexts and target-owned breakpoints for a new HELLO;
CONTINUE restores all breakpoint bytes before resuming. The reconnect implementation is present,
but repeated reconnect/loss qualification is not yet a required hosted lane. No proprietary KDNET
module is needed for these scenarios.
