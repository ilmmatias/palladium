# Palladium agent guidance

## Scope and orientation

Palladium is a from-scratch modular/hybrid, NT-inspired operating system. It is not NT source- or
binary-compatible; preserve its own abstractions instead of importing Unix/Linux conventions.

- `src/boot/osloader` is the amd64 UEFI loader and PE image loader.
- `src/kernel` contains the executive, HAL, KD, memory manager, object/event systems, scheduler,
  threading, and display support.
- `src/drivers/acpi` is the project's custom ACPI/AML implementation.
- `src/debugger` is the Python host client for the custom kernel-debugger protocol.
- `src/sdk` provides shared headers plus boot, kernel, and user CRT/RT/crypt variants.

Start with `README.md`; use `docs/amd64/AddressSpace.txt` for the current four-level address-space
layout. Public interfaces live under `src/kernel/include/public`,
`src/drivers/acpi/include/public`, `src/sdk/*/include`, and `src/sdk/include`. Export lists in
`*.def`, packed loader/protocol structures, PE structures, and architecture context layouts are ABI
surfaces, not implementation details.

## Build and environment

The only accepted target is currently `amd64` (`x86_64` to compiler-rt). The build requires CMake
3.21+, Ninja, a full Clang/LLVM toolchain (Clang, LLD, `llvm-ar`, and `llvm-windres`), and the
bare-metal compiler-rt builtins installed where `src/sdk/cmake/project.cmake` expects them.

The compiler-rt bootstrap used by CI is equivalent to:

```sh
PALLADIUM_PATH="$PWD" COMPILER_RT_PATH=/path/to/llvm-project/compiler-rt \
COMPILER_RT_TARGET=x86_64 LLVM_SUFFIX=-21 ./build-compiler-rt.sh
```

The script creates `build.compiler-rt` and uses `sudo` to install under `/usr/local`; do not run it
without explicit approval for privileged host modification. `LLVM_SUFFIX` is optional when using
unsuffixed LLVM tools.

Canonical configure/build commands are:

```sh
cmake -S src -B build.amd64.rel -G Ninja -DCMAKE_BUILD_TYPE=Release -DARCH=amd64
cmake --build build.amd64.rel
```

Add `-DLLVM_SUFFIX=-21` for the CI toolchain. The project is C23, freestanding where selected by
the target kind, PE/COFF-oriented, `-Wall -Wextra -Werror`, and built without host libraries.
Release/MinSizeRel enable LTO; Debug/RelWithDebInfo do not. Cleanly reconfigure when changing
architecture, compiler suffix, build type, toolchain logic, compile definitions, or ABI-affecting
flags; use a clean build when stale LTO or generated CMake state could hide a result.

For clang-tidy, configure with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`, then use the documented command:

```sh
run-clang-tidy -p build.amd64.rel -source-filter='.*\.(c)$'
```

Do not apply `-fix` across unrelated code. Format changed project-owned C sources and headers with the root
`.clang-format`; the EDK2-derived headers under `src/boot/osloader/include/efi` deliberately disable
clang-format and retain their upstream style and license.

## Images, QEMU, and verification

There is no CTest, unit-test, or other automated test suite in this repository. CI builds Release
with LLVM 21, creates a FAT EFI image and ISO, and uploads them; it does not boot them. At minimum,
build every touched target. Changes to boot, kernel, HAL, drivers, runtime startup, loader-facing
ABI, or KD should also be exercised through the affected amd64 QEMU scenario when practical.

From the build directory, the documented boot flow is:

```sh
OVMF_CODE_PATH=/path/to/OVMF_CODE.fd OVMF_VARS_PATH=/path/to/OVMF_VARS.fd \
BOOT_DRIVERS="acpi" ../run.sh
```

`run.sh` requires Bash, mtools, mkisofs/cdrtools, `qemu-system-x86_64`, Linux KVM, and host CPU
passthrough. It creates/replaces `fat.img` and `iso9660.iso`, copies OVMF to `code.bin`/`vars.bin`
on first use, and subsequently lets QEMU mutate `vars.bin`. With debugging enabled it adds host
port forwarding and writes `net0.pcap`. Review environment variables in `README.md` and the script
before use; do not run it from a directory containing valuable files with those names. Pass extra
QEMU arguments only deliberately.

A successful smoke boot should reach the kernel build banner, memory-management message, processor
online count, and ACPI driver's `enabled ACPI` message, with no loader error or fatal-error screen.
Debugger-enabled boot intentionally waits during early initialization until the client connects:

```sh
python3 -m src.debugger localhost <port>
```

Hardware-only behavior, malformed firmware breadth, SMP races, and many debugger paths are not
covered automatically; report such gaps rather than implying coverage.

## Engineering and change discipline

- Follow the established subsystem prefixes and public/private split. Inspect declarations,
  callers, `.def` exports, and cross-subsystem consumers before changing a shared interface.
- Keep generic code architecture-neutral. Put amd64 mechanisms in existing `amd64` paths and keep
  paired C/assembly constants and layouts synchronized. Do not imply another architecture works.
- Preserve explicit-width fields, overflow/bounds checks, packing, alignment, calling conventions,
  and PE/UEFI contracts. Never silently change a loader block, packet, exported structure, context
  frame, on-disk image layout, or symbol list.
- Treat external input (PE images, boot configuration, ACPI/AML, network packets) as malformed until
  validated. Use recoverable return/status paths for expected bad input or allocation failure;
  reserve assertions/fatal errors for documented nonrecoverable kernel or initialization invariants.
- In privilege- or concurrency-sensitive code, establish the current IRQL, interrupt state, held
  locks, ownership, lifetime, and allowed allocation behavior from local declarations and callers.
  Do not invent a lock order or assume a routine is reentrant when the repository does not say so.
- Preserve matching allocation/free, map/unmap, acquire/release, packet get/release, and
  reference/dereference pairs on every success and failure path. Use the same four-byte pool tag.
- Keep protocol logic separate from TUI/presentation, transport framing, and external module
  loading. Prefer a focused behavior change over a broad rewrite; separate refactoring when
  practical.
- Preserve custom implementations, including ACPI/AML, CRT/RT, PE loading, and KD. Do not replace
  the ACPI stack with ACPICA or uACPI unless explicitly directed.
- Add or update focused tests when infrastructure exists; otherwise document and perform the
  narrowest reproducible build/QEMU/debugger scenario. Update authoritative docs for new public
  interfaces, boot/runtime requirements, or architecture contracts, not for incidental refactors.

## Source and artifact hygiene

Never commit `build.*`, `build.compiler-rt`, `_root`, `__pycache__`, CMake/Ninja output, PE binaries,
EFI/FAT/ISO images, OVMF copies or variable stores, `boot.cfg`, logs, or packet captures. Do not
propagate the machine-specific absolute path currently present in `.vscode/settings.json`; use the
CLI commands above as portable guidance.

KDNET extensibility modules are proprietary, externally supplied artifacts. They must come from a
legally obtained supported Windows installation, remain outside source control, and be supplied at
runtime through `BOOT_DEBUG_DRIVER_PREFIX`/`BOOT_DEBUG_DRIVERS`. Never add binaries, secrets,
firmware blobs, or machine-local paths to repository guidance.

## Definition of done

- The requested behavior is complete without unrelated rewrites or generated artifacts.
- Relevant targets build cleanly with warnings as errors; clang-tidy is run when appropriate.
- The affected image/QEMU/debugger or hardware scenario is exercised when practical, and its
  diagnostics are checked.
- ABI/protocol/export/packed-layout and amd64 impact are reviewed explicitly.
- Ownership, cleanup, IRQL, interrupt, lock, and malformed-input paths are rechecked where relevant.
- Public behavior or architecture documentation is updated when required.
- The handoff lists commands run, results, tests not run, and remaining risks.
