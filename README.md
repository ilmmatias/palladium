# The Palladium Operating System

## About

Palladium is a modular/hybrid NT-inspired (but not binary nor source compatible) kernel and operating system.

## How to build and run this?

To build the source, you need to have a full LLVM toolchain, ninja, and cmake installed.

LLVM comes with cross-compiling support by default, and you only need to rebuild compiler-rt for the baremetal target as of yet.
This can be done using the [build-compiler-rt.sh](./build-compiler-rt.sh) script, as such: `COMPILER_RT_PATH=llvm-project/compiler-rt COMPILER_RT_TARGET=x86_64 ./build-compiler-rt.sh`.

An example of how to build the OS (after having the baremetal compiler-rt) would be: `cmake -Ssrc -Bbuild.amd64.rel -GNinja -DCMAKE_BUILD_TYPE=Release -DARCH=amd64 && cmake --build build.amd64.rel`.

After this, we provide [run.sh](./run.sh) to create and run a test ISO on QEMU (x86-64/amd64 only). This script requires mtools and mkisofs (cdrtools). There is also a set of required environment variables; In specific, `BOOT_DRIVERS=<space separated list...>` (which should contain at least `acpi`), `OVMF_CODE_PATH=<path to the OVMF ROM file>`, and `OVMF_VARS_PATH=<path to the OVMF vars file>`. You can also configure remote debugging support using `BOOT_DEBUG_ENABLED=true/false`, `BOOT_DEBUG_ECHO_ENABLED=<true/false, depending on if you want the KdPrint messages to show on the target display as well>`, `BOOT_DEBUG_ADDRESS=x.x.x.x`, `BOOT_DEBUG_PORT=<uint16 value>`, `BOOT_DEBUG_DRIVER_PREFIX=<where to find the KDNET extensibility modules>`, and `BOOT_DEBUG_DRIVERS=<space separated list of what KDNET extensibility modules to copy>`. The specific network device that will be used by QEMU when `BOOT_DEBUG_ENABLED=true` can also be configured via `BOOT_DEBUG_DEVICE`. For example, you could run `pushd build.amd64.rel && OVMF_CODE_PATH=<path> OVMF_VARS_PATH=<path> BOOT_DRIVERS="acpi" ../run.sh && popd` after building the OS.

The KDNET extensibility modules from `BOOT_DEBUG_DRIVERS` should be extracted from a legally obtained Windows system, in specific Windows 10 version 19H2 or newer, but preferably Windows 11 version 24H2, as that is the version tested during development (for the extra network card compatibility).

When `BOOT_DEBUG_ENABLED` is `true`, the kernel will attempt to connect to the remote debugger during the early initialization (and wait until the connection is established). A simple Python 3 tool is included at `src/debugger` for connecting with the kernel. It currently only depends on curses, and can be run as such: `python3 -m src.debugger <ip> <port>`, with the `<ip>` matching the `BOOT_DEBUG_ADDRESS` value (or `localhost`, when running under QEMU).

For amd64 (the only currently supported configuration), use COMPILER_RT_TARGET=`x86_64` when building compiler-rt, and `ARCH=amd64` when building the OS.

## Contributing

Issues and pull requests are welcome!
