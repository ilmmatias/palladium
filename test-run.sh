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
mkdir -p _root/Boot
mkdir -p _root/System

cat <<EOF >_root/Boot/bootmgr.ini
DefaultSelection=BootInstall
Timeout=5

[BootInstall]
Type=palladium
Text=Boot from the Installation Disk
Icon=boot()/Boot/install.bmp
SystemFolder=boot()/System
Drivers=[
    acpi.sys
    pci.sys
]

[Test]
Type=chainload
Text=Boot from the First Hard Disk
Icon=boot()/Boot/os.bmp
Path=bios(80)
EOF

cp icons/* _root/Boot/
cp build.boot/boot/bootsect/iso9660boot.com _root/Boot/iso9660boot.com
cat build.boot/boot/bootsect/startup.com build.boot/boot/bootmgr/bootmgr.exe > _root/bootmgr
cp build.target/kernel/kernel.exe _root/System/kernel.exe
cp build.target/drivers/acpi/acpi.sys _root/System/acpi.sys
cp build.target/drivers/pci/pci.sys _root/System/pci.sys

if [ "$1" == "fat32" ]
then
    sudo rm -rf /mnt/mount
    sudo mkdir -p /mnt/mount
    dd if=/dev/zero of=build.target/fat32.img count=64 bs=1M 2>/dev/null
    echo ',,0c,*' | sfdisk build.target/fat32.img 1>/dev/null
    sudo losetup -P /dev/loop32 build.target/fat32.img
    sudo /usr/sbin/mkfs.fat -F32 /dev/loop32p1 1>/dev/null
    sudo mount -t vfat -o loop /dev/loop32p1 /mnt/mount 1>/dev/null
    sudo cp _root/bootmgr /mnt/mount/bootmgr 1>/dev/null
    sudo cp -Rf _root/Boot /mnt/mount/Boot 1>/dev/null
    sudo cp -Rf _root/System /mnt/mount/System 1>/dev/null
    sudo umount /mnt/mount 1>/dev/null
    sudo dd if=build.boot/boot/bootsect/fat32boot.com of=/dev/loop32p1 seek=90 skip=90 count=422 bs=1 conv=notrunc 2>/dev/null
    sudo losetup -d /dev/loop32
    dd if=build.boot/boot/bootsect/mbrboot.com of=build.target/fat32.img count=440 bs=1 conv=notrunc 2>/dev/null
    echo "[5/5] Running emulator"
    qemu-system-x86_64 -enable-kvm -M smm=off -cpu host -smp 4 -drive file=build.target/fat32.img,index=0,media=disk,format=raw -no-reboot $2 1>/dev/null
elif [ "$1" == "exfat" ]
then
    sudo rm -rf /mnt/mount
    sudo mkdir -p /mnt/mount
    dd if=/dev/zero of=build.target/exfat.img count=64 bs=1M 2>/dev/null
    echo ',,7,*' | sfdisk build.target/exfat.img 1>/dev/null
    sudo losetup -P /dev/loop32 build.target/exfat.img
    sudo mkfs.exfat /dev/loop32p1 1>/dev/null
    sudo mount -t exfat /dev/loop32p1 /mnt/mount 1>/dev/null
    sudo cp _root/bootmgr /mnt/mount/bootmgr 1>/dev/null
    sudo cp -Rf _root/Boot /mnt/mount/Boot 1>/dev/null
    sudo cp -Rf _root/System /mnt/mount/System 1>/dev/null
    sudo umount /mnt/mount 1>/dev/null
    sudo dd if=build.boot/boot/bootsect/exfatboot.com of=/dev/loop32p1 seek=120 skip=120 count=392 bs=1 conv=notrunc 2>/dev/null
    sudo dd if=build.boot/boot/bootsect/exfatboot.com of=/dev/loop32p1 seek=512 skip=512 bs=1 conv=notrunc 2>/dev/null
    sudo losetup -d /dev/loop32
    dd if=build.boot/boot/bootsect/mbrboot.com of=build.target/exfat.img count=440 bs=1 conv=notrunc 2>/dev/null
    echo "[5/5] Running emulator"
    qemu-system-x86_64 -enable-kvm -M smm=off -cpu host -smp 4 -drive file=build.target/exfat.img,index=0,media=disk,format=raw -no-reboot $2 1>/dev/null
elif [ "$1" == "ntfs" ]
then
    sudo rm -rf /mnt/mount
    sudo mkdir -p /mnt/mount
    dd if=/dev/zero of=build.target/ntfs.img count=64 bs=1M 2>/dev/null
    echo ',,7,*' | sfdisk build.target/ntfs.img 1>/dev/null
    sudo losetup -P /dev/loop32 build.target/ntfs.img
    sudo mkfs.ntfs -p2048 /dev/loop32p1 1>/dev/null 2>&1
    sudo mount -t ntfs /dev/loop32p1 /mnt/mount 1>/dev/null
    sudo cp _root/bootmgr /mnt/mount/bootmgr 1>/dev/null
    sudo cp -Rf _root/Boot /mnt/mount/Boot 1>/dev/null
    sudo cp -Rf _root/System /mnt/mount/System 1>/dev/null
    sudo umount /mnt/mount 1>/dev/null
    sudo dd if=build.boot/boot/bootsect/ntfsboot.com of=/dev/loop32p1 seek=80 skip=80 count=426 bs=1 conv=notrunc 2>/dev/null
    sudo dd if=build.boot/boot/bootsect/ntfsboot.com of=/dev/loop32p1 seek=512 skip=512 bs=1 conv=notrunc 2>/dev/null
    sudo losetup -d /dev/loop32
    dd if=build.boot/boot/bootsect/mbrboot.com of=build.target/ntfs.img count=440 bs=1 conv=notrunc 2>/dev/null
    echo "[5/5] Running emulator"
    qemu-system-x86_64 -enable-kvm -M smm=off -cpu host -smp 4 -drive file=build.target/ntfs.img,index=0,media=disk,format=raw -no-reboot $2 1>/dev/null
else
    mkisofs -iso-level 2 -R -b Boot/iso9660boot.com -no-emul-boot -o build.target/iso9660.iso _root 1>/dev/null 2>&1
    echo "[5/5] Running emulator"
    qemu-system-x86_64 -enable-kvm -M smm=off -cpu host -smp 4 -m 4G -cdrom build.target/iso9660.iso -no-reboot $2 1>/dev/null
fi

rm -rf _root
