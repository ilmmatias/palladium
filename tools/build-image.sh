#!/usr/bin/env bash
# SPDX-FileCopyrightText: (C) 2026 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

build_dir=
output_dir=
work_dir=

size=64M
default_drivers=true
debug_enabled=false
debug_echo_enabled=false
debug_address=
debug_port=

declare -a drivers=()
declare -a debug_modules=()

die() {
    printf 'Error: %s\n' "$1" >&2
    exit 1
}

usage_error() {
    printf 'Error: %s\n\n' "$1" >&2
    usage >&2
    exit 2
}

# Validate that an option has a non-empty value
# Arguments: option_name, option_value
require_value() {
    if (($# < 2)) || [[ -z $2 ]]; then
        usage_error "$1 requires a value"
    fi

    echo "$2"
}

usage() {
    cat <<'EOF_USAGE'
Usage: tools/build-image.sh --build-dir DIR --output-dir DIR [options]

Build a FAT32 UEFI image and an El Torito ISO.

Options:
  --size SIZE               Use an explicit truncate-style size for the FAT32 image
  --no-default-boot-drivers Do not include the default boot drivers
  --boot-driver NAME[=PATH] Add a specific boot driver to the list
  --debug-enabled           Add DebugEnabled=true
  --debug-echo-enabled      Add DebugEchoEnabled=true
  --debug-address ADDRESS   Add DebugAddress=ADDRESS
  --debug-port PORT         Add DebugPort=PORT
  --debug-module PATH       Copy and use a specific debugger module
  --help                    Show this dialog
EOF_USAGE
}

# Parse command-line arguments into the script configuration
parse_args() {
    while (($#)); do
        case $1 in
            --build-dir) build_dir=$(require_value "$@"); shift 2 ;;
            --output-dir) output_dir=$(require_value "$@"); shift 2 ;;
            --size) size=$(require_value "$@"); shift 2 ;;
            --no-default-boot-drivers) default_drivers=false; shift ;;
            --boot-driver) drivers+=("$(require_value "$@")"); shift 2 ;;
            --debug-enabled) debug_enabled=true; shift ;;
            --debug-echo-enabled) debug_echo_enabled=true; shift ;;
            --debug-address) debug_address=$(require_value "$@"); shift 2 ;;
            --debug-port) debug_port=$(require_value "$@"); shift 2 ;;
            --debug-module) debug_modules+=("require_value "$@""); shift 2 ;;
            --help) usage; exit 0 ;;
            *) usage_error "unknown argument: $1" ;;
        esac
    done
}

# Check whether all required host tools are available
check_host() {
    local tool

    for tool in mformat mmd mcopy mkisofs realpath truncate; do
        command -v "$tool" >/dev/null || die "required tool not found: $tool"
    done
}

# Validate and canonicalize the build directory
check_build_dir() {
    [[ -d $build_dir ]] || die "build directory not found: $build_dir"
    build_dir=$(realpath -- "$build_dir")

    if [[ ! -f $build_dir/boot/osloader/osloader.exe || ! -f $build_dir/kernel/kernel.exe ]]; then
        die "build directory lacks osloader.exe or kernel.exe: $build_dir"
    fi
}

# Create, validate, and canonicalize the output directory
check_output_dir() {
    if [[ -e $output_dir && ! -d $output_dir ]]; then
        die "output path is not a directory: $output_dir"
    fi

    mkdir -p -- "$output_dir"
    output_dir=$(realpath -- "$output_dir")
}

# Remove the temporary workspace without changing the script's exit status
cleanup() {
    local status=$?

    trap - EXIT
    if [[ -n ${work_dir:-} ]]; then
        rm -rf -- "$work_dir" || true
    fi

    exit "$status"
}

# Create a temporary workspace alongside the final output files
prepare_workspace() {
    work_dir=$(mktemp -d "$output_dir/.build-image.XXXXXX")
    trap cleanup EXIT
}

# Create and format an empty FAT32 image
# Arguments: image
create_image() {
    local image=$1

    truncate -s "$size" -- "$image"
    mformat -F -i "$image" ::
}

# Copy the bootloader and kernel and initialize the boot configuration
# Arguments: image, config
copy_basic_files() {
    local image=$1
    local config=$2

    mmd -i "$image" ::/EFI ::/EFI/BOOT ::/EFI/PALLADIUM
    mcopy -i "$image" "$build_dir/boot/osloader/osloader.exe" ::/EFI/BOOT/BOOTX64.EFI
    mcopy -i "$image" "$build_dir/kernel/kernel.exe" ::/EFI/PALLADIUM/KERNEL.EXE

    printf 'Kernel=KERNEL.EXE\n' >"$config"
}

# Install one boot driver and append its boot configuration entry
# Arguments: image, config, spec, seen_ref
install_driver() {
    local image=$1
    local config=$2
    local spec=$3
    local -n seen_ref=$4
    local name path

    name=${spec%%=*}
    if [[ $spec == *=* ]]; then
        path=${spec#*=}
    else
        path=$build_dir/drivers/${name,,}/${name,,}.sys
    fi

    name=${name^^}
    [[ $name =~ ^[A-Z0-9_-]+$ ]] || die "invalid boot driver name: $name"
    [[ -f $path ]] || die "boot driver not found: $path"
    [[ -z ${seen_ref[$name]+x} ]] || die "duplicate boot driver: $name"

    seen_ref[$name]=1
    mcopy -i "$image" "$path" "::/EFI/PALLADIUM/$name.SYS"
    printf 'BootDriver=%s.SYS\n' "$name" >>"$config"
}

# Install the default and user-specified boot drivers
# Arguments: image, config
copy_drivers() {
    local image=$1
    local config=$2
    local spec
    local -a effective_drivers=("${drivers[@]}")
    local -A seen=()

    if [[ $default_drivers == true ]]; then
        effective_drivers=("ACPI=$build_dir/drivers/acpi/acpi.sys" "${effective_drivers[@]}")
    fi

    for spec in "${effective_drivers[@]}"; do
        install_driver "$image" "$config" "$spec" seen
    done
}

# Append debugger settings and copy debugger modules
# Arguments: image, config
setup_debug() {
    local image=$1
    local config=$2
    local module module_name
    local -A seen=()

    [[ $debug_enabled == false ]] || printf 'DebugEnabled=true\n' >>"$config"
    [[ $debug_echo_enabled == false ]] || printf 'DebugEchoEnabled=true\n' >>"$config"
    [[ -z $debug_address ]] || printf 'DebugAddress=%s\n' "$debug_address" >>"$config"

    if [[ -n $debug_port ]]; then
        if [[ ! $debug_port =~ ^[0-9]+$ ]] ||
           ((${#debug_port} > 5)) || ((10#$debug_port < 1 || 10#$debug_port > 65535)); then
            die "debug port must be between 1 and 65535"
        fi

        printf 'DebugPort=%s\n' "$debug_port" >>"$config"
    fi

    for module in "${debug_modules[@]}"; do
        [[ -f $module ]] || die "debug module not found: $module"

        module_name=${module##*/}
        module_name=${module_name^^}
        [[ -z ${seen[$module_name]+x} ]] || die "duplicate debug module: $module_name"

        seen[$module_name]=1
        mcopy -i "$image" "$module" "::/EFI/PALLADIUM/$module_name"
    done
}

# Copy the completed boot configuration into the FAT image
# Arguments: image, config
finalize_image() {
    local image=$1
    local config=$2

    mcopy -i "$image" "$config" ::/EFI/PALLADIUM/BOOT.CFG
}

# Wrap the FAT image in an El Torito ISO
# Arguments: image, iso
create_iso() {
    local image=$1
    local iso=$2
    local iso_root=$work_dir/iso-root

    mkdir -- "$iso_root"
    cp -- "$image" "$iso_root/fat.img"
    mkisofs -quiet -R -f -eltorito-boot fat.img -no-emul-boot -o "$iso" "$iso_root"
}

# Move the completed artifacts into the output directory
# Arguments: image, iso
move_outputs() {
    local image=$1
    local iso=$2

    mv -- "$image" "$output_dir/fat.img"
    mv -- "$iso" "$output_dir/iso9660.iso"
}

# Build and move the FAT32 image and El Torito ISO
main() {
    local image iso config

    parse_args "$@"
    if [[ -z $build_dir || -z $output_dir ]]; then
        usage_error '--build-dir and --output-dir are required'
    fi

    check_host
    check_build_dir
    check_output_dir
    prepare_workspace

    image=$work_dir/fat.img
    iso=$work_dir/iso9660.iso
    config=$work_dir/boot.cfg

    create_image "$image"
    copy_basic_files "$image" "$config"
    copy_drivers "$image" "$config"
    setup_debug "$image" "$config"
    finalize_image "$image" "$config"
    create_iso "$image" "$iso"
    move_outputs "$image" "$iso"
}

main "$@"
