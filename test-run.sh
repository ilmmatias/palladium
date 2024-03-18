#!/bin/bash

echo "[1/4] Building host tools"

if [[ ! -d build.host ]]
then
    mkdir build.host
    cd build.host
    cmake ../src/sdk/host -GNinja -DCMAKE_BUILD_TYPE=Release
else
    cd build.host
fi

cmake --build .
cd ..

echo "[2/4] Building system"

if [[ ! -d build.target ]]
then
    mkdir build.target
    cd build.target
    cmake ../src -GNinja -DARCH=amd64 -DCMAKE_BUILD_TYPE=Release
else
    cd build.target
fi

cmake --build .
cd ..

echo "[3/4] Creating bootable image"
dd if=/dev/zero of=build.target/fat.img bs=1k count=184320 1>/dev/null 2>&1
mformat -F -i build.target/fat.img
mmd -i build.target/fat.img ::/EFI
mmd -i build.target/fat.img ::/EFI/BOOT
mmd -i build.target/fat.img ::/EFI/PALLADIUM
mcopy -i build.target/fat.img build.target/boot/osloader/osloader.exe ::/EFI/BOOT/BOOTX64.EFI
mcopy -i build.target/fat.img build.target/kernel/kernel.exe ::/EFI/PALLADIUM/KERNEL.EXE
mcopy -i build.target/fat.img build.target/drivers/acpi/acpi.sys ::/EFI/PALLADIUM/ACPI.SYS
mkdir -p _root
cp build.target/fat.img _root/fat.img
mkisofs -R -f -eltorito-boot fat.img -no-emul-boot -o build.target/iso9660.iso _root 1>/dev/null 2>&1
rm -rf _root

echo "[4/4] Running emulator"
qemu-system-x86_64 -enable-kvm -M smm=off -cpu host -smp $(nproc) -drive if=pflash,format=raw,unit=0,file=/usr/share/ovmf/OVMF.fd,readonly=on -cdrom build.target/iso9660.iso -no-reboot $*
