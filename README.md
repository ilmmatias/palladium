# The Palladium Operating System

## About

Palladium is a modular/hybrid NT-inspired (but not binary nor source compatible) kernel and operating system.

## How to build and run this?

To build the source, you need to have a full LLVM toolchain, ninja, and cmake installed.

LLVM comes with cross-compiling support by default, and you only need to rebuild compiler-rt for the baremetal target as of yet.
This can be done using the [build-compiler-rt.sh](./build-compiler-rt.sh) script, as such: `COMPILER_RT_PATH=llvm-project/compiler-rt COMPILER_RT_TARGET=x86_64 ./build-compiler-rt.sh`.

An example of how to build the OS (after having the baremetal compiler-rt) would be: `cmake -Ssrc -Bbuild.amd64.rel -GNinja -DCMAKE_BUILD_TYPE=Release -DARCH=amd64 && cmake --build build.amd64.rel`.

After this, you can test the OS on QEMU using the [run.sh](./run.sh) script, as such: `pushd build.amd64.rel && OVMF_CODE_PATH=/usr/share/ovmf/OVMF.fd ../run.sh && popd`. The run.sh script requires mtools and mkisofs (cdrtools), as well as QEMU.

For amd64 (the only currently supported configuration), use COMPILER_RT_TARGET=`x86_64` when building compiler-rt, and `ARCH=amd64` when building the OS.

## Contributing

Issues and pull requests are welcome!
