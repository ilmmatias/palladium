#!/bin/bash

dd if=/dev/zero of=fat.img bs=1k count=33792 1>/dev/null 2>&1
mformat -F -i fat.img
mmd -i fat.img ::/EFI
mmd -i fat.img ::/EFI/BOOT
mmd -i fat.img ::/EFI/PALLADIUM
mcopy -i fat.img boot/osloader/osloader.exe ::/EFI/BOOT/BOOTX64.EFI
mcopy -i fat.img kernel/kernel.exe ::/EFI/PALLADIUM/KERNEL.EXE
mcopy -i fat.img drivers/acpi/acpi.sys ::/EFI/PALLADIUM/ACPI.SYS
mkdir -p _root
cp fat.img _root/fat.img
mkisofs -R -f -eltorito-boot fat.img -no-emul-boot -o iso9660.iso _root 1>/dev/null 2>&1
rm -rf _root
qemu-system-x86_64 -enable-kvm -cpu host -smp $(nproc) -drive if=pflash,format=raw,unit=0,file=$OVMF_CODE_PATH,readonly=on -cdrom iso9660.iso -no-reboot -no-shutdown $*
