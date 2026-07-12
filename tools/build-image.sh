#!/usr/bin/env bash
# SPDX-FileCopyrightText: (C) 2026 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

usage() {
    cat <<'EOF'
Usage: tools/build-image.sh --build-dir DIR --output-dir DIR [options]

Build a 64 MiB FAT32 UEFI image and an El Torito ISO.

Options:
  --boot-driver NAME[=PATH] Add a built or explicitly located boot driver
  --no-default-acpi         Do not add the default ACPI boot driver
  --debug-enabled           Add DebugEnabled=true
  --debug-echo-enabled      Add DebugEchoEnabled=true
  --debug-address ADDRESS   Add DebugAddress=ADDRESS
  --debug-port PORT         Add DebugPort=PORT
  --debug-module PATH       Copy an explicitly named local debugger module
  --diagnostic-serial       Enable PC16550 diagnostics at 0x3F8/115200
  -h, --help                Show this help
EOF
}

build_dir=
output_dir=
default_acpi=true
debug_enabled=false
debug_echo_enabled=false
debug_address=
debug_port=
diagnostic_serial=false
declare -a drivers=()
declare -a debug_modules=()

while (($#)); do
    case "$1" in
        --build-dir) build_dir=${2-}; shift 2 ;;
        --output-dir) output_dir=${2-}; shift 2 ;;
        --boot-driver) drivers+=("${2-}"); shift 2 ;;
        --no-default-acpi) default_acpi=false; shift ;;
        --debug-enabled) debug_enabled=true; shift ;;
        --debug-echo-enabled) debug_echo_enabled=true; shift ;;
        --debug-address) debug_address=${2-}; shift 2 ;;
        --debug-port) debug_port=${2-}; shift 2 ;;
        --debug-module) debug_modules+=("${2-}"); shift 2 ;;
        --diagnostic-serial) diagnostic_serial=true; shift ;;
        -h|--help) usage; exit 0 ;;
        *) printf 'unknown argument: %s\n' "$1" >&2; usage >&2; exit 2 ;;
    esac
done

if [[ -z $build_dir || -z $output_dir ]]; then
    usage >&2
    exit 2
fi
for tool in mformat mmd mcopy mkisofs realpath truncate; do
    command -v "$tool" >/dev/null || { printf 'required tool not found: %s\n' "$tool" >&2; exit 1; }
done

build_dir=$(realpath "$build_dir")
if [[ ! -f $build_dir/boot/osloader/osloader.exe || ! -f $build_dir/kernel/kernel.exe ]]; then
    printf 'build directory lacks osloader.exe or kernel.exe: %s\n' "$build_dir" >&2
    exit 1
fi
if [[ -e $output_dir ]]; then
    [[ -d $output_dir ]] || { printf 'output path is not a directory: %s\n' "$output_dir" >&2; exit 1; }
    if [[ -n $(find "$output_dir" -mindepth 1 -maxdepth 1 -print -quit) ]]; then
        printf 'refusing to use nonempty output directory: %s\n' "$output_dir" >&2
        exit 1
    fi
else
    mkdir -p "$output_dir"
fi
output_dir=$(realpath "$output_dir")

if [[ $default_acpi == true ]]; then
    drivers=("ACPI=$build_dir/drivers/acpi/acpi.sys" "${drivers[@]}")
fi

image=$output_dir/fat.img
iso=$output_dir/iso9660.iso
config=$output_dir/boot.cfg
truncate -s 64M "$image"
mformat -F -i "$image" ::
mmd -i "$image" ::/EFI ::/EFI/BOOT ::/EFI/PALLADIUM
mcopy -i "$image" "$build_dir/boot/osloader/osloader.exe" ::/EFI/BOOT/BOOTX64.EFI
mcopy -i "$image" "$build_dir/kernel/kernel.exe" ::/EFI/PALLADIUM/KERNEL.EXE

printf 'Kernel=KERNEL.EXE\n' >"$config"
declare -A seen=()
for spec in "${drivers[@]}"; do
    name=${spec%%=*}
    if [[ $spec == *=* ]]; then
        path=${spec#*=}
    else
        path=$build_dir/drivers/${name,,}/${name,,}.sys
    fi
    name=${name^^}
    [[ $name =~ ^[A-Z0-9_-]+$ ]] || { printf 'invalid boot driver name: %s\n' "$name" >&2; exit 2; }
    [[ -f $path ]] || { printf 'boot driver not found: %s\n' "$path" >&2; exit 1; }
    [[ -z ${seen[$name]+x} ]] || { printf 'duplicate boot driver: %s\n' "$name" >&2; exit 2; }
    seen[$name]=1
    mcopy -i "$image" "$path" "::/EFI/PALLADIUM/$name.SYS"
    printf 'BootDriver=%s.SYS\n' "$name" >>"$config"
done

[[ $debug_enabled == false ]] || printf 'DebugEnabled=true\n' >>"$config"
[[ $debug_echo_enabled == false ]] || printf 'DebugEchoEnabled=true\n' >>"$config"
if [[ $diagnostic_serial == true ]]; then
    printf 'DiagnosticDevice=PC16550\n' >>"$config"
    printf 'DiagnosticAddress=0x3F8\n' >>"$config"
    printf 'DiagnosticBaudRate=115200\n' >>"$config"
fi
[[ -z $debug_address ]] || printf 'DebugAddress=%s\n' "$debug_address" >>"$config"
if [[ -n $debug_port ]]; then
    [[ $debug_port =~ ^[0-9]+$ ]] && ((debug_port >= 1 && debug_port <= 65535)) || {
        printf 'debug port must be between 1 and 65535\n' >&2; exit 2;
    }
    printf 'DebugPort=%s\n' "$debug_port" >>"$config"
fi
for module in "${debug_modules[@]}"; do
    [[ -f $module ]] || { printf 'debug module not found: %s\n' "$module" >&2; exit 1; }
    mcopy -i "$image" "$module" "::/EFI/PALLADIUM/$(basename "$module" | tr '[:lower:]' '[:upper:]')"
done
mcopy -i "$image" "$config" ::/EFI/PALLADIUM/BOOT.CFG

stage=$(mktemp -d "$output_dir/.iso-root.XXXXXX")
cleanup() {
    rm -rf -- "$stage"
}
trap cleanup EXIT
cp "$image" "$stage/fat.img"
mkisofs -quiet -R -f -eltorito-boot fat.img -no-emul-boot -o "$iso" "$stage"
printf 'created %s and %s\n' "$image" "$iso"
