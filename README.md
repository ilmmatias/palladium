# The Palladium Operating System

## About

Palladium is my personal pet project, which has gone through multiple years of rewrites, changing names, changing build systems, and a few other things.  
At the moment we have no GUI, or nothing big other than a "Hello, World!" being printed to the screen after we boot, but we do already have some things working.  

## What already works

1. All the groundwork for the build system (which uses cmake) is already in place, and we can build at least the boot and sdk modules (which are the only ones we have at the moment).
2. We have boot sectors for ISO9660, FAT32, and exFAT; I've already experimented with EFI boot and booting other filesystems on BIOS, but for now those are the ones available in the source tree.
3. The startup module (which bootstraps us and gets all the PE sections loaded into the right place) is already functional, and bootmgr.exe can be loaded (though it doesn't do much yet).

## How to build and run this?

To build the source, you need to have clang, [jwasm](https://github.com/Baron-von-Riedesel/JWasm), and cmake installed.

We currently only support the amd64 target (the x86 target is only used for building the bootloader), so make sure to create the llvm-ml and llvm-ml64 symlinks in your /usr/bin folder if your distro doesn't ship with them (as we use MASM syntax on x86/amd64).  
The lld-link symlink is also required (for all targets).

If you want to use the test-run.sh script, you also need qemu on top dosfstools and mtools (for fat32), exfatprogs and your distro's version of exfat-fuse (for exfat), or mkisofs/genisoimage for iso9660.

## Contributing

Issues and pull requests are welcome!
