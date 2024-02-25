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
dd if=/dev/zero of=build.target/fat.img bs=1k count=1440 1>/dev/null 2>&1
sudo mkfs.vfat build.target/fat.img 1>/dev/null
sudo mount -t vfat build.target/fat.img /mnt/mount
sudo mkdir -p /mnt/mount/EFI/BOOT
sudo cp build.target/boot/osloader/osloader.exe /mnt/mount/EFI/BOOT/BOOTX64.EFI
sudo mkdir -p /mnt/mount/EFI/PALLADIUM
sudo cp build.target/kernel/kernel.exe /mnt/mount/EFI/PALLADIUM/KERNEL.EXE
sudo cp build.target/drivers/acpi/acpi.sys /mnt/mount/EFI/PALLADIUM/ACPI.SYS
sudo cp build.target/drivers/pci/pci.sys /mnt/mount/EFI/PALLADIUM/PCI.SYS
sudo umount /mnt/mount
mkdir -p _root
cp build.target/fat.img _root/fat.img
mkisofs -R -f -eltorito-boot fat.img -no-emul-boot -o build.target/iso9660.iso _root 1>/dev/null 2>&1
rm -rf _root

echo "[4/4] Running emulator"
qemu-system-x86_64 -enable-kvm -M smm=off -cpu host -smp $(nproc) -drive if=pflash,format=raw,unit=0,file=/usr/share/ovmf/OVMF.fd,readonly=on -cdrom build.target/iso9660.iso -no-reboot $*
