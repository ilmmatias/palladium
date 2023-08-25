#!/bin/bash

echo "[1/5] Building host tools"

if [[ ! -d obj.host ]]
then
    mkdir obj.host
    cd obj.host
    cmake ../src/sdk/host -GNinja -DCMAKE_BUILD_TYPE=Debug
else
    cd obj.host
fi

cmake --build .
cd ..

echo "[2/5] Building bootsector"

if [[ ! -d obj.x86 ]]
then
    mkdir obj.x86
    cd obj.x86
    cmake ../src -GNinja -DARCH=x86 -DCMAKE_BUILD_TYPE=Debug
else
    cd obj.x86
fi

cmake --build .
cd ..

echo "[3/5] Building system"

if [[ ! -d obj.amd64 ]]
then
    mkdir obj.amd64
    cd obj.amd64
    cmake ../src -GNinja -DARCH=amd64 -DCMAKE_BUILD_TYPE=Debug
else
    cd obj.amd64
fi

cmake --build .
cd ..

echo "[4/5] Creating bootable image"
mkdir -p _root/System
obj.host/create-boot-registry
cp obj.x86/boot/bootsect/iso9660boot.com _root/iso9660boot.com
cat obj.x86/boot/startup/startup.com obj.x86/boot/bootmgr/bootmgr.exe > _root/bootmgr
cp obj.amd64/kernel/kernel.exe _root/System/kernel.exe
cp obj.amd64/drivers/exfat/exfat.sys _root/System/exfat.sys
cp obj.amd64/drivers/fat32/fat32.sys _root/System/fat32.sys
cp obj.amd64/drivers/iso9660/iso9660.sys _root/System/iso9660.sys
cp obj.amd64/drivers/ntfs/ntfs.sys _root/System/ntfs.sys

if [ "$1" == "fat32" ]
then
    dd if=/dev/zero of=obj.amd64/fat32.img count=64 bs=1M 2>/dev/null
    /usr/sbin/mkfs.fat -F32 obj.amd64/fat32.img 1>/dev/null
    dd if=obj.x86/boot/bootsect/fat32boot.com of=obj.amd64/fat32.img seek=90 skip=90 count=422 bs=1 conv=notrunc 2>/dev/null
    mcopy -i obj.amd64/fat32.img _root/bootmgr ::/ 1>/dev/null
    mcopy -i obj.amd64/fat32.img _root/bootmgr.reg ::/ 1>/dev/null
    mcopy -i obj.amd64/fat32.img _root/System ::/ 1>/dev/null
    echo "[5/5] Running emulator"
    qemu-system-x86_64 -enable-kvm -M smm=off -cpu host -drive file=obj.amd64/fat32.img,index=0,media=disk,format=raw -no-reboot 1>/dev/null
elif [ "$1" == "exfat" ]
then
    sudo rm -rf /mnt/mount
    sudo mkdir -p /mnt/mount
    dd if=/dev/zero of=obj.amd64/exfat.img count=64 bs=1M 2>/dev/null
    sudo mkfs.exfat obj.amd64/exfat.img 1>/dev/null
    dd if=obj.x86/boot/bootsect/exfatboot.com of=obj.amd64/exfat.img seek=120 skip=120 count=392 bs=1 conv=notrunc 2>/dev/null
    dd if=obj.x86/boot/bootsect/exfatboot.com of=obj.amd64/exfat.img seek=512 skip=512 bs=1 conv=notrunc 2>/dev/null
    sudo fsck.exfat -y obj.amd64/exfat.img 1>/dev/null 2>&1
    sudo mount -t exfat -o loop obj.amd64/exfat.img /mnt/mount 1>/dev/null
    sudo cp _root/bootmgr /mnt/mount/bootmgr 1>/dev/null
    sudo cp _root/bootmgr.reg /mnt/mount/bootmgr.reg 1>/dev/null
    sudo cp -Rf _root/System /mnt/mount/System 1>/dev/null
    sudo umount /mnt/mount 1>/dev/null
    echo "[5/5] Running emulator"
    qemu-system-x86_64 -enable-kvm -M smm=off -cpu host -drive file=obj.amd64/exfat.img,index=0,media=disk,format=raw -no-reboot 1>/dev/null
elif [ "$1" == "ntfs" ]
then
    sudo rm -rf /mnt/mount
    sudo mkdir -p /mnt/mount
    dd if=/dev/zero of=obj.amd64/ntfs.img count=64 bs=1M 2>/dev/null
    sudo mkfs.ntfs -F obj.amd64/ntfs.img 1>/dev/null 2>&1
    dd if=obj.x86/boot/bootsect/ntfsboot.com of=obj.amd64/ntfs.img seek=80 skip=80 count=426 bs=1 conv=notrunc 2>/dev/null
    dd if=obj.x86/boot/bootsect/ntfsboot.com of=obj.amd64/ntfs.img seek=512 skip=512 bs=1 conv=notrunc 2>/dev/null
    sudo mount -t ntfs -o loop obj.amd64/ntfs.img /mnt/mount 1>/dev/null
    sudo cp _root/bootmgr /mnt/mount/bootmgr 1>/dev/null
    sudo cp _root/bootmgr.reg /mnt/mount/bootmgr.reg 1>/dev/null
    sudo cp -Rf _root/System /mnt/mount/System 1>/dev/null
    sudo umount /mnt/mount 1>/dev/null
    echo "[5/5] Running emulator"
    qemu-system-x86_64 -enable-kvm -M smm=off -cpu host -drive file=obj.amd64/ntfs.img,index=0,media=disk,format=raw -no-reboot 1>/dev/null
else
    mkisofs -iso-level 2 -R -b iso9660boot.com -no-emul-boot -o obj.amd64/iso9660.iso _root 1>/dev/null 2>&1
    echo "[5/5] Running emulator"
    qemu-system-x86_64 -enable-kvm -M smm=off -cpu host -cdrom obj.amd64/iso9660.iso -no-reboot 1>/dev/null
fi

rm -rf _root
