# The Palladium Operating System

## About

Palladium is a modular/hybrid NT-inspired (but not binary nor source compatible) kernel and operating system.

## How to build and run this?

To build the source, you need CMake 3.21+, Ninja, a full Clang/LLVM toolchain, and the bare-metal
compiler-rt builtins. LLVM supplies the cross compiler; build and install the builtins with:

```sh
PALLADIUM_PATH="$PWD" COMPILER_RT_PATH=/path/to/llvm-project/compiler-rt \
COMPILER_RT_TARGET=x86_64 ./build-compiler-rt.sh
```

The only supported target is currently amd64:

```sh
cmake -S src -B build.amd64.rel -G Ninja -DCMAKE_BUILD_TYPE=Release -DARCH=amd64
cmake --build build.amd64.rel
```

For clang-tidy, configure with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`, then run:

```sh
run-clang-tidy -p build.amd64.rel -source-filter='.*\.(c)$'
```

Do not apply clang-tidy `-fix` across unrelated source. The LLVM-versioned warning classification
is recorded in [the M1 baseline](docs/roadmap/M1ClangTidyBaseline.md). The selected analyzer gate is:

```sh
tools/run-clang-tidy-high-signal.sh build.amd64.rel
```

### Images and reproducible QEMU smoke boot

Image construction requires Bash, mtools, and mkisofs/cdrtools. It never mounts the image or needs
privilege:

```sh
tools/build-image.sh \
  --build-dir build.amd64.rel \
  --output-dir build.amd64.rel/images \
  --diagnostic-serial
```

The output directory must be absent or empty. The command creates a 64-MiB FAT image, ISO, and the
generated `boot.cfg`. `--diagnostic-serial` enables the opt-in PC16550 diagnostic sink used by the
headless smoke runner; ordinary images remain framebuffer-only unless configured.

The corresponding boot configuration is:

```text
DiagnosticDevice=PC16550
DiagnosticAddress=0x3F8
DiagnosticBaudRate=115200
```

For the one-command proprietary-free evidence lane, build the image and boot it headlessly with:

```sh
tools/smoke.sh \
  --build-dir build.amd64.rel \
  --output-dir build.amd64.rel/evidence \
  --ovmf-code /path/to/OVMF_CODE.fd \
  --ovmf-vars /path/to/OVMF_VARS.fd
```

The smoke runner requires Python 3, QEMU, and matching OVMF code/variable templates:

```sh
tools/run-qemu.py smoke \
  --image build.amd64.rel/images/iso9660.iso \
  --ovmf-code /path/to/OVMF_CODE.fd \
  --ovmf-vars /path/to/OVMF_VARS.fd \
  --output-dir build.amd64.rel/qemu-smoke
```

It uses a fixed one-CPU TCG profile, a fresh variable store, a 60-second timeout, ordered loader/
kernel/MM/processor/ACPI markers, and QMP shutdown. Logs and `result.json` remain in the output
directory on success or failure. Before changing the required CI gate, exercise its repeat mode:

```sh
tools/run-qemu.py ... --repeat 20
```

For the historical interactive workflow, run `run.sh` from the build directory with
`OVMF_CODE_PATH`, `OVMF_VARS_PATH`, and an absent/empty `PALLADIUM_RUN_OUTPUT`. Set
`BOOT_DIAGNOSTIC_SERIAL=true` to retain serial diagnostics. The wrapper uses KVM and the host CPU
when available, otherwise TCG, and gives every run a fresh OVMF variable store.

The runner can also be invoked directly. Deliberate extra QEMU arguments belong after `--` and are
accepted only by the interactive profile:

```sh
tools/run-qemu.py interactive \
  --image build.amd64.rel/images/iso9660.iso \
  --ovmf-code /path/to/OVMF_CODE.fd \
  --ovmf-vars /path/to/OVMF_VARS.fd \
  --output-dir build.amd64.rel/qemu-interactive \
  -- -device e1000
```

### M1 host and kernel self-tests

Portable CRT/RT tests compile the production C sources directly with Palladium headers taking
precedence, builtins disabled, and ASan/UBSan enabled:

```sh
cmake -S tests/host -B build.host -G Ninja
cmake --build build.host
ctest --test-dir build.host --output-on-failure
```

Private kernel self-tests are available only in a dedicated RelWithDebInfo image. The option is off
by default, so ordinary kernels do not contain the test entry points or fault/test state:

```sh
cmake -S src -B build.amd64.selftest -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DARCH=amd64 \
  -DPALLADIUM_ENABLE_SELF_TESTS=ON
cmake --build build.amd64.selftest
tools/build-image.sh \
  --build-dir build.amd64.selftest \
  --output-dir build.amd64.selftest/images \
  --diagnostic-serial
tools/run-qemu.py selftest \
  --image build.amd64.selftest/images/iso9660.iso \
  --ovmf-code /path/to/OVMF_CODE.fd \
  --ovmf-vars /path/to/OVMF_VARS.fd \
  --output-dir build.amd64.selftest/qemu-selftest \
  --smp 1
```

The self-test profile accepts only `--smp 1`, `2`, or `4`, uses TCG and a 120-second timeout, and
requires the structured `M1TEST` records to complete without a failure or kernel stop. The 2- and
4-CPU profiles are qualification lanes rather than substitutes for hardware concurrency testing.
The contracts and current rollout status are recorded in
[the M1 executive baseline](docs/M1ExecutiveBaseline.md).

### Kernel debugger client

The Python debugger uses curses for its TUI. Capstone is loaded only when disassembly is requested.
It is therefore optional for non-disassembly headless use; install the Python `capstone` package to
use `dp`/`dv`.
The existing TUI invocation remains:

```sh
python3 -m src.debugger <ipv4-or-localhost> <port>
```

Headless mode accepts standard input, a UTF-8 script, or repeatable commands and emits stable text
or versioned JSONL:

```sh
python3 -m src.debugger localhost 50005 --headless --format jsonl \
  --command 'rp/b16 ffff800000000000' --command quit
python3 -m src.debugger localhost 50005 --headless --script commands.kd
```

Run its host tests with:

```sh
python3 -m unittest discover -s src/debugger/tests
```

The fake UDP target tests the Python codec/session and malformed-datagram behavior only. It does not
exercise the kernel's raw Ethernet/KD extension path.

The implementation and qualification record for this milestone is in
[the M0 evidence-plane notes](docs/M0EvidencePlane.md).

When `BOOT_DEBUG_ENABLED=true`, the kernel intentionally waits for the client during early
initialization. The current KDNET extensibility modules must be extracted from a legally obtained
supported Windows installation, supplied explicitly through `BOOT_DEBUG_DRIVER_PREFIX` and
`BOOT_DEBUG_DRIVERS`, and kept outside source control. The image builder and CI never download or
enable them by default.

For example, a local interactive debug image can be built without copying the module into the
repository:

```sh
cd build.amd64.rel
OVMF_CODE_PATH=/path/to/OVMF_CODE.fd \
OVMF_VARS_PATH=/path/to/OVMF_VARS.fd \
BOOT_DEBUG_ENABLED=true \
BOOT_DEBUG_DRIVER_PREFIX=/path/to/legally-obtained/modules \
BOOT_DEBUG_DRIVERS='kd_02_8086.dll' \
../run.sh
```

## Contributing

Issues and pull requests are welcome!
