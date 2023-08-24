# The Palladium Operating System

## About

Palladium is my personal pet project, which has gone through multiple years of rewrites, changing names, changing build systems, and a few other things.  
At the moment we have no GUI, or nothing big other than a "Hello, World!" being printed to the screen after we boot, but we do already have some things working.  

## What already works

1.  We have boot sectors for ISO9660, FAT32, exFAT and NTFS; I've already experimented with EFI boot and booting other filesystems on BIOS, but for now those are the ones available in the source tree.
3.  The boot manager (composed of the startup module and bootmgr.exe) is already functional, and can load up the kernel (while appying KASLR to the base address).

## How to build and run this?

To build the source, you need to have clang, [jwasm](https://github.com/Baron-von-Riedesel/JWasm), and cmake installed.

We assume your distro's LLVM build won't come with the right compiler-rt targets built, so you'll need to rebuild compiler-rt like described [here](https://llvm.org/docs/HowToCrossCompileBuiltinsOnArm.html).  
You can use the example cmake configuration available [here](./compiler_rt-flags.txt).  
We use w64-mingw32 targets, such as i686-w64-mingw32 and x86_64-w64-mingw32.

We currently only support the amd64 target (the x86 target is only used for building the bootloader), so make sure to create the llvm-ml and llvm-ml64 symlinks in your /usr/bin folder if your distro doesn't ship with them (as we use MASM syntax on x86/amd64).  
The lld-link symlink is also required (for all targets).

If you want to use the test-run.sh script, you also need qemu on top dosfstools and mtools (for fat32), exfatprogs and your distro's version of exfat-fuse (for exfat), or mkisofs/genisoimage for iso9660.

## Contributing

Issues and pull requests are welcome!
