#!/bin/bash

# Not having at least ACPI.SYS for generic amd64 platforms makes no sense
# (and as this is building an image for QEMU/KVM, we're very much generic).
if [ -z "$BOOT_DRIVERS" ]; then
    printf "The boot driver list (\$BOOT_DRIVERS) is not set.\n"
    printf "This is probably unintended, as it is required for the system to boot.\n"
    exit 1
fi

# Increase the block count as we add more stuff, just make sure it doesn't go under ~33MiB!
IMAGE_NAME=fat.img
IMAGE_BLOCK_SIZE=1024
IMAGE_BLOCK_COUNT=33792
dd if=/dev/zero of=$IMAGE_NAME bs=$IMAGE_BLOCK_SIZE count=$IMAGE_BLOCK_COUNT 1>/dev/null 2>&1

# FAT32 is required for EFI booting (and we only support EFI booting, at least on generic amd64).
# As long as the image is big enough, mformat should automatically chose FAT32 instead of FAT16,
# so we should be safe.
mformat -F -i $IMAGE_NAME
mmd -i $IMAGE_NAME ::/EFI
mmd -i $IMAGE_NAME ::/EFI/BOOT
mmd -i $IMAGE_NAME ::/EFI/PALLADIUM

# Basic/essential system components, always on fixed paths.
mcopy -i $IMAGE_NAME boot/osloader/osloader.exe ::/EFI/BOOT/BOOTX64.EFI
mcopy -i $IMAGE_NAME kernel/kernel.exe ::/EFI/PALLADIUM/KERNEL.EXE

# This is kinda hack-ish, and maybe we should do it in another way?
declare -A DRIVER_PATH
declare -a VALID_BOOT_DRIVERS
DRIVER_PATH["ACPI"]=drivers/acpi/acpi.sys
for driver in $BOOT_DRIVERS; do
    if [[ ! -n "${DRIVER_PATH[${driver^^}]}" ]]; then
        printf "Skipping invalid driver ${driver^^} in \$BOOT_DRIVERS...\n"
    elif [[ ${VALID_BOOT_DRIVERS[*]} =~ ${driver^^} ]]; then
        printf "Skipping duplicate driver ${driver^^} in \$BOOT_DRIVERS...\n"
    else
        mcopy -i $IMAGE_NAME "${DRIVER_PATH[${driver^^}]}" ::/EFI/PALLADIUM/${driver^^}.SYS
        VALID_BOOT_DRIVERS+=("$driver")
    fi
done

# Copy in any KDNET drivers (these need to be directly extracted from a valid Windows
# installation).
for driver in $BOOT_DEBUG_DRIVERS; do
    if [[ ! -f $BOOT_DEBUG_DRIVER_PREFIX/$driver ]]; then
        printf "Skipping invalid driver ${driver^^} in \$BOOT_DEBUG_DRIVERS...\n"
    else
        mcopy -i $IMAGE_NAME $BOOT_DEBUG_DRIVER_PREFIX/$driver ::/EFI/PALLADIUM/${driver^^}
    fi
done

# Build the boot config (append new stuff to this section as needed).
echo "Kernel=KERNEL.EXE" > boot.cfg

if [[ "${BOOT_DEBUG_ENABLED}" == "true" ]]; then
    echo "DebugEnabled=true" >> "boot.cfg"
fi

if [[ "${BOOT_DEBUG_ECHO_ENABLED}" == "true" ]]; then
    echo "DebugEchoEnabled=true" >> "boot.cfg"
fi

if [[ -n "${BOOT_DEBUG_ADDRESS}" ]]; then
    echo "DebugAddress=${BOOT_DEBUG_ADDRESS}" >> "boot.cfg"
fi

if [[ -n "${BOOT_DEBUG_PORT}" ]]; then
    echo "DebugPort=${BOOT_DEBUG_PORT}" >> "boot.cfg"
fi

for driver in "${VALID_BOOT_DRIVERS[@]}"; do
    echo "BootDriver=${driver^^}.SYS" >> boot.cfg
done

mcopy -i $IMAGE_NAME boot.cfg ::/EFI/PALLADIUM/BOOT.CFG
rm -f boot.cfg

# We'll be booting QEMU via a CDROM image, so mkisofs it out of the EFI boot image.
ISO_NAME=iso9660.iso
mkdir -p _root
cp $IMAGE_NAME _root/fat.img
mkisofs -R -f -eltorito-boot $IMAGE_NAME -no-emul-boot -o $ISO_NAME _root 1>/dev/null 2>&1
rm -rf _root

# Copy in the OVMF blobs if they don't exist yet (the OVMF_CODE/VARS_PATH variables
# only need to be set when initially doing this).
if [ ! -f "code.bin" ] || [ ! -f "vars.bin" ]; then
    if [ -z "$OVMF_CODE_PATH" ] || [ ! -f "$OVMF_CODE_PATH" ]; then
        printf "The OVMF_CODE_PATH variable is not set.\n"
        printf "This is required for the first spin up of the QEMU instance.\n"
        exit 1
    elif [ -z "$OVMF_VARS_PATH" ] || [ ! -f "$OVMF_VARS_PATH" ]; then
        printf "The OVMF_VARS_PATH variable is not set.\n"
        printf "This is required for the first spin up of the QEMU instance.\n"
        exit 1
    else
        cp "$OVMF_CODE_PATH" code.bin
        cp "$OVMF_VARS_PATH" vars.bin
    fi
fi

# Spin up a new virtual machine.
QEMU_BINARY="qemu-system-x86_64"
QEMU_BASIC_FLAGS="--enable-kvm -cpu host,+invtsc -smp $(nproc)"
QEMU_OVMF_CODE_FLAGS="-drive if=pflash,format=raw,unit=0,file=code.bin,readonly=on"
QEMU_OVMF_VARS_FLAGS="-drive if=pflash,format=raw,unit=1,file=vars.bin"
QEMU_DISK_FLAGS="-cdrom $ISO_NAME"
QEMU_EXTRA_FLAGS="-no-reboot -no-shutdown"

# Only enable networking if debugging is enabled too.
# (And also make sure to adjust hostfwd depending on BOOT_DEBUG_PORT!)
if [[ "${BOOT_DEBUG_ENABLED}" == "true" ]]; then
    if [[ -n "${BOOT_DEBUG_PORT}" ]]; then
        QEMU_NETWORK_FLAGS="-netdev user,id=net0,hostfwd=udp::${BOOT_DEBUG_PORT}-:${BOOT_DEBUG_PORT}"
    else
        QEMU_NETWORK_FLAGS="-netdev user,id=net0,hostfwd=tcp::50005-:50005"
    fi

    if [[ -n "${BOOT_DEBUG_DEVICE}" ]]; then
        QEMU_NETWORK_FLAGS="$QEMU_NETWORK_FLAGS -device ${BOOT_DEBUG_DEVICE},netdev=net0"
    else
        QEMU_NETWORK_FLAGS="$QEMU_NETWORK_FLAGS -device e1000,netdev=net0"
    fi

    QEMU_NETWORK_FLAGS="$QEMU_NETWORK_FLAGS -object filter-dump,id=f0,netdev=net0,file=net0.pcap"
else
    QEMU_NETWORK_FLAGS=""
fi

$QEMU_BINARY \
    $QEMU_BASIC_FLAGS \
    $QEMU_OVMF_CODE_FLAGS $QEMU_OVMF_VARS_FLAGS \
    $QEMU_DISK_FLAGS \
    $QEMU_NETWORK_FLAGS \
    $QEMU_EXTRA_FLAGS \
    $*
