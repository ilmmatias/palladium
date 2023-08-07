#!/bin/bash

if [[ ! -d obj.x86 ]]
then
    mkdir obj.x86
    cd obj.x86
    cmake ../src -DARCH=x86 -DCMAKE_BUILD_TYPE=Debug
else
    cd obj.x86
fi

cmake --build .
cd ..

if [[ ! -d obj.amd64 ]]
then
    mkdir obj.amd64
    cd obj.amd64
    cmake ../src -DARCH=amd64 -DCMAKE_BUILD_TYPE=Debug
else
    cd obj.amd64
fi

cmake --build .
cd ..

mkdir -p _root
cp obj.x86/boot/bootsect/iso9660boot.com _root/iso9660boot.com
cat obj.x86/boot/startup/startup.com obj.x86/boot/bootmgr/bootmgr.exe > _root/bootmgr

if [ "$1" == "fat32" ]
then
    dd if=/dev/zero of=obj.amd64/fat32.img count=64 bs=1M 2>/dev/null
    sudo mkfs.fat -F32 obj.amd64/fat32.img 1>/dev/null
    mcopy -i obj.amd64/fat32.img _root/bootmgr ::/ 1>/dev/null
    dd if=obj.x86/boot/bootsect/fat32boot.com of=obj.amd64/fat32.img seek=90 skip=90 count=422 bs=1 conv=notrunc 2>/dev/null
    qemu-system-x86_64 -M smm=off -hda obj.amd64/fat32.img -no-reboot -d int
elif [ "$1" == "exfat" ]
then
    sudo rm -rf /mnt/mount
	sudo mkdir -p /mnt/mount
    dd if=/dev/zero of=obj.amd64/exfat.img count=64 bs=1M 2>/dev/null
    sudo mkfs.exfat obj.amd64/exfat.img 1>/dev/null
    dd if=obj.x86/boot/bootsect/exfatboot.com of=obj.amd64/exfat.img seek=120 skip=120 count=392 bs=1 conv=notrunc 2>/dev/null
    dd if=obj.x86/boot/bootsect/exfatboot.com of=obj.amd64/exfat.img seek=512 skip=512 count=512 bs=1 conv=notrunc 2>/dev/null
    sudo fsck.exfat -y obj.amd64/exfat.img
    sudo mount -t exfat -o loop obj.amd64/exfat.img /mnt/mount 1>/dev/null
    sudo cp _root/bootmgr /mnt/mount/bootmgr 1>/dev/null
    sudo mkdir /mnt/mount/open_this 1>/dev/null
    printf '25 Thompson 56987' | sudo tee /mnt/mount/open_this/flag.txt
    sudo umount /mnt/mount 1>/dev/null
    qemu-system-x86_64 -M smm=off -hda obj.amd64/exfat.img -no-reboot -d int
else
    mkisofs -R -b iso9660boot.com -no-emul-boot -o obj.amd64/iso9660.iso _root
    qemu-system-x86_64 -M smm=off -cdrom obj.amd64/iso9660.iso -no-reboot -d int
fi

rm -rf _root
