#!/bin/bash

echo "[1/5] Building host tools"

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

echo "[2/5] Building bootsector"

if [[ ! -d build.boot ]]
then
    mkdir build.boot
    cd build.boot
    # Always build the bootsector as release/minsize, we need to fit it within around 200KiB!
    cmake ../src -GNinja -DARCH=x86 -DCMAKE_BUILD_TYPE=Release
else
    cd build.boot
fi

cmake --build .
cd ..

echo "[3/5] Building system"

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

echo "[4/5] Creating bootable image"
mkdir -p _root/System
build.host/create-boot-registry
cp build.boot/boot/bootsect/iso9660boot.com _root/iso9660boot.com
cat build.boot/boot/startup/startup.com build.boot/boot/bootmgr/bootmgr.exe > _root/bootmgr
cp build.target/kernel/kernel.exe _root/System/kernel.exe
cp build.target/drivers/acpi/acpi.sys _root/System/acpi.sys
cp build.target/drivers/pci/pci.sys _root/System/pci.sys

if [ "$1" == "fat32" ]
then
    dd if=/dev/zero of=build.target/fat32.img count=64 bs=1M 2>/dev/null
    /usr/sbin/mkfs.fat -F32 build.target/fat32.img 1>/dev/null
    mcopy -i build.target/fat32.img _root/bootmgr ::/ 1>/dev/null
    mcopy -i build.target/fat32.img _root/bootmgr.reg ::/ 1>/dev/null
    mcopy -i build.target/fat32.img _root/System ::/ 1>/dev/null
    dd if=build.boot/boot/bootsect/fat32boot.com of=build.target/fat32.img seek=90 skip=90 count=422 bs=1 conv=notrunc 2>/dev/null
    echo "[5/5] Running emulator"
    qemu-system-x86_64 -enable-kvm -M smm=off -cpu host -smp 4 -drive file=build.target/fat32.img,index=0,media=disk,format=raw -no-reboot 1>/dev/null
elif [ "$1" == "exfat" ]
then
    sudo rm -rf /mnt/mount
    sudo mkdir -p /mnt/mount
    dd if=/dev/zero of=build.target/exfat.img count=64 bs=1M 2>/dev/null
    sudo mkfs.exfat build.target/exfat.img 1>/dev/null
    sudo mount -t exfat -o loop build.target/exfat.img /mnt/mount 1>/dev/null
    sudo cp _root/bootmgr /mnt/mount/bootmgr 1>/dev/null
    sudo cp _root/bootmgr.reg /mnt/mount/bootmgr.reg 1>/dev/null
    sudo cp -Rf _root/System /mnt/mount/System 1>/dev/null
    sudo umount /mnt/mount 1>/dev/null
    dd if=build.boot/boot/bootsect/exfatboot.com of=build.target/exfat.img seek=120 skip=120 count=392 bs=1 conv=notrunc 2>/dev/null
    dd if=build.boot/boot/bootsect/exfatboot.com of=build.target/exfat.img seek=512 skip=512 bs=1 conv=notrunc 2>/dev/null
    echo "[5/5] Running emulator"
    qemu-system-x86_64 -enable-kvm -M smm=off -cpu host -smp 4 -drive file=build.target/exfat.img,index=0,media=disk,format=raw -no-reboot 1>/dev/null
elif [ "$1" == "ntfs" ]
then
    sudo rm -rf /mnt/mount
    sudo mkdir -p /mnt/mount
    dd if=/dev/zero of=build.target/ntfs.img count=64 bs=1M 2>/dev/null
    sudo mkfs.ntfs -F build.target/ntfs.img 1>/dev/null 2>&1
    sudo mount -t ntfs -o loop build.target/ntfs.img /mnt/mount 1>/dev/null
    sudo cp _root/bootmgr /mnt/mount/bootmgr 1>/dev/null
    sudo cp _root/bootmgr.reg /mnt/mount/bootmgr.reg 1>/dev/null
    sudo cp -Rf _root/System /mnt/mount/System 1>/dev/null
    sudo umount /mnt/mount 1>/dev/null
    dd if=build.boot/boot/bootsect/ntfsboot.com of=build.target/ntfs.img seek=80 skip=80 count=426 bs=1 conv=notrunc 2>/dev/null
    dd if=build.boot/boot/bootsect/ntfsboot.com of=build.target/ntfs.img seek=512 skip=512 bs=1 conv=notrunc 2>/dev/null
    echo "[5/5] Running emulator"
    qemu-system-x86_64 -enable-kvm -M smm=off -cpu host -smp 4 -drive file=build.target/ntfs.img,index=0,media=disk,format=raw -no-reboot 1>/dev/null
else
    mkisofs -iso-level 2 -R -b iso9660boot.com -no-emul-boot -o build.target/iso9660.iso _root 1>/dev/null 2>&1
    echo "[5/5] Running emulator"
    qemu-system-x86_64 -enable-kvm -M smm=off -cpu host -smp 4 -cdrom build.target/iso9660.iso -d cpu -no-reboot
fi

rm -rf _root
