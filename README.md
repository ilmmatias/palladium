# The Palladium Operating System

## About

Palladium is a modular, hybrid, NT-inspired kernel and operating system, currently
supporting amd64 systems using UEFI.

## Prerequisites

Building Palladium requires:

- A complete LLVM toolchain
- CMake and Ninja
- The LLVM compiler-rt sources
- mtools and `mkisofs` for creating boot images
- QEMU and OVMF for running the resulting image locally

LLVM supports cross-compilation out of the box, but compiler-rt must currently be rebuilt for
Palladium's bare-metal target.

## Build compiler-rt

Use [`tools/build-compiler-rt.sh`](./tools/build-compiler-rt.sh) to configure, build, and install
compiler-rt:

```sh
PALLADIUM_PATH="$PWD" \
COMPILER_RT_PATH=/path/to/llvm-project/compiler-rt \
COMPILER_RT_TARGET=x86_64 \
tools/build-compiler-rt.sh
```

Set `LLVM_SUFFIX` when the installed LLVM tools use a version suffix, such as `-22`:

```sh
LLVM_SUFFIX=-22 \
PALLADIUM_PATH="$PWD" \
COMPILER_RT_PATH=/path/to/llvm-project/compiler-rt \
COMPILER_RT_TARGET=x86_64 \
tools/build-compiler-rt.sh
```

The installation step uses `sudo`.

## Build Palladium

Configure and build an amd64 release:

```sh
cmake \
    -S src \
    -B build.amd64.rel \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DARCH=amd64

cmake --build build.amd64.rel
```

Add `-DLLVM_SUFFIX=-22` when using version-suffixed LLVM tools.

## Build boot images

Use [`tools/build-image.sh`](./tools/build-image.sh) to create a FAT32 UEFI image and an El Torito
ISO:

```sh
tools/build-image.sh \
    --build-dir build.amd64.rel \
    --output-dir build.amd64.rel/images
```

This produces:

```text
build.amd64.rel/images/fat.img
build.amd64.rel/images/iso9660.iso
```

The default amd64 boot driver set currently contains ACPI. Additional drivers can be included using
`--boot-driver NAME[=PATH]`. When `PATH` is omitted, the script looks for
`drivers/<name>/<name>.sys` inside the build directory.

Debugger configuration and support modules can be added while building the image:

```sh
tools/build-image.sh \
    --build-dir build.amd64.rel \
    --output-dir build.amd64.rel/images \
    --debug-enabled \
    --debug-echo-enabled \
    --debug-address ADDRESS \
    --debug-port PORT \
    --debug-module /path/to/module.dll
```

Run `tools/build-image.sh --help` for the complete option list.

KDNET extensibility modules must be extracted from a legally obtained Windows installation. Windows
10 version 19H2 or newer should work, while Windows 11 version 24H2 and newer are the versions used
during development and provide compatibility with additional network devices.

## Run under QEMU

Use [`tools/run-qemu.sh`](./tools/run-qemu.sh) to start the generated ISO under QEMU/KVM. On the
first run, provide OVMF code and variable files to be copied in:

```sh
tools/run-qemu.sh \
    --iso build.amd64.rel/images/iso9660.iso \
    --ovmf-code /path/to/OVMF_CODE.fd \
    --ovmf-vars /path/to/OVMF_VARS.fd
```

The script copies them to persistent `code.bin` and `vars.bin` files beside the ISO. Later
runs can omit `--ovmf-code` and `--ovmf-vars`:

```sh
tools/run-qemu.sh --iso build.amd64.rel/iso9660.iso
```

Additional QEMU arguments can be passed after `--`:

```sh
tools/run-qemu.sh \
    --iso build.amd64.rel/iso9660.iso \
    -- -serial stdio
```

When the image was built with debugger support, enable the matching QEMU network interface using
`--debug-enabled` and `--debug-port PORT`. The network device defaults to `e1000` and can be changed
with `--debug-device DEVICE`.

The debugger client is implemented in Python and currently depends only on curses:

```sh
python3 -m src.debugger ADDRESS PORT
```

Under QEMU, `ADDRESS` can normally be `localhost`. The kernel waits for the debugger connection
during early initialization when `DebugEnabled=true` is present in its boot configuration.

Run `tools/run-qemu.sh --help` for the complete option list.

## Static analysis

Configure CMake with `CMAKE_EXPORT_COMPILE_COMMANDS=ON`, then run clang-tidy over the C sources:

```sh
run-clang-tidy \
    -p build.amd64.rel \
    -source-filter='.*\.(c)$'
```

To apply supported fixes automatically:

```sh
run-clang-tidy \
    -fix \
    -p build.amd64.rel \
    -source-filter='.*\.(c)$'
```

## Contributing

Issues and pull requests are welcome!
