#!/bin/bash

echo "[1/4] Building bootsector"

if [[ ! -d obj.x86 ]]
then
    mkdir obj.x86
    cd obj.x86
    cmake ../src -GNinja -DARCH=x86 -DCMAKE_BUILD_TYPE=Debug 1>/dev/null
else
    cd obj.x86
fi

cmake --build .
cd ..

echo "[2/4] Building system"

if [[ ! -d obj.amd64 ]]
then
    mkdir obj.amd64
    cd obj.amd64
    cmake ../src -GNinja -DARCH=amd64 -DCMAKE_BUILD_TYPE=Debug 1>/dev/null
else
    cd obj.amd64
fi

cmake --build .
cd ..

echo "[3/4] Creating bootable image"
mkdir -p _root
cp obj.x86/boot/bootsect/iso9660boot.com _root/iso9660boot.com
cat obj.x86/boot/startup/startup.com obj.x86/boot/bootmgr/bootmgr.exe > _root/bootmgr

if [ "$1" == "fat32" ]
then
    mkdir -p _root/open_this
    printf 'Hello, World! This file has been loaded using the boot FAT32 driver.\n' | tee _root/open_this/flag.txt 1>/dev/null
    dd if=/dev/zero of=obj.amd64/fat32.img count=64 bs=1M 2>/dev/null
    /usr/sbin/mkfs.fat -F32 obj.amd64/fat32.img 1>/dev/null
    dd if=obj.x86/boot/bootsect/fat32boot.com of=obj.amd64/fat32.img seek=90 skip=90 count=422 bs=1 conv=notrunc 2>/dev/null
    mcopy -i obj.amd64/fat32.img _root/bootmgr ::/ 1>/dev/null
    mcopy -i obj.amd64/fat32.img _root/open_this ::/ 1>/dev/null
    echo "[4/4] Running emulator"
    qemu-system-x86_64 -M smm=off -drive file=obj.amd64/fat32.img,index=0,media=disk,format=raw -no-reboot 1>/dev/null
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
    sudo mkdir -p /mnt/mount/open_this
    printf 'Hello, World! This file has been loaded using the boot exFAT driver.\n' | sudo tee /mnt/mount/open_this/flag.txt 1>/dev/null
    sudo umount /mnt/mount 1>/dev/null
    echo "[4/4] Running emulator"
    qemu-system-x86_64 -M smm=off -drive file=obj.amd64/exfat.img,index=0,media=disk,format=raw -no-reboot 1>/dev/null
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
    sudo mkdir -p /mnt/mount/open_this
    printf 'Hello, World! This file has been loaded using the boot NTFS driver.\n' | sudo tee /mnt/mount/open_this/flag.txt 1>/dev/null
    sudo umount /mnt/mount 1>/dev/null
    echo "[4/4] Running emulator"
    qemu-system-x86_64 -M smm=off -drive file=obj.amd64/ntfs.img,index=0,media=disk,format=raw -no-reboot 1>/dev/null
else
    mkdir -p _root/open_this
    printf 'Hello, World! This file has been loaded using the boot ISO9660 driver.\n' | tee _root/open_this/flag.txt 1>/dev/null
    mkisofs -iso-level 2 -R -b iso9660boot.com -no-emul-boot -o obj.amd64/iso9660.iso _root
    echo "[4/4] Running emulator"
    qemu-system-x86_64 -M smm=off -cdrom obj.amd64/iso9660.iso -no-reboot 1>/dev/null
fi

rm -rf _root
