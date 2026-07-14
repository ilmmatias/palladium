#!/usr/bin/env bash
# SPDX-FileCopyrightText: (C) 2026 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

iso=
state_dir=
ovmf_code_source=
ovmf_vars_source=
qemu_binary=qemu-system-x86_64

debug_enabled=false
debug_port=
debug_device=e1000
debug_capture=
kvm_enabled=true

declare -a qemu_extra_args=()

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
Usage: tools/run-qemu.sh --iso FILE [options] [-- QEMU_OPTIONS...]

Run the specified image under QEMU/KVM + OVMF.

Options:
  --state-dir DIR       Store OVMF state beside the ISO by default
  --ovmf-code FILE      Initialize code.bin from this OVMF code image
  --ovmf-vars FILE      Initialize vars.bin from this OVMF variables image
  --qemu BINARY         Use an explicit QEMU binary
  --no-kvm              Do not enable KVM acceleration
  --debug-enabled       Enable the debugger network interface
  --debug-port PORT     Forward this UDP port and enable debugger networking
  --debug-device DEVICE Use this QEMU network device, default: e1000
  --debug-capture FILE  Write debugger network traffic to this file
  --help                Show this dialog
EOF_USAGE
}

# Parse command-line arguments into the script configuration
parse_args() {
    while (($#)); do
        case $1 in
            --iso) iso=$(require_value "$@"); shift 2 ;;
            --state-dir) state_dir=$(require_value "$@"); shift 2 ;;
            --ovmf-code) ovmf_code_source=$(require_value "$@"); shift 2 ;;
            --ovmf-vars) ovmf_vars_source=$(require_value "$@"); shift 2 ;;
            --qemu) qemu_binary=$(require_value "$@"); shift 2 ;;
            --no-kvm) kvm_enabled=false; shift ;;
            --debug-enabled) debug_enabled=true; shift ;;
            --debug-port) debug_port=$(require_value "$@"); shift 2 ;;
            --debug-device) debug_device=$(require_value "$@"); shift 2 ;;
            --debug-capture) debug_capture=$(require_value "$@"); shift 2 ;;
            --help) usage; exit 0 ;;
            --) shift; qemu_extra_args+=("$@"); break ;;
            *) usage_error "unknown argument: $1" ;;
        esac
    done
}

# Validate and canonicalize the ISO and state directory
check_paths() {
    [[ -f $iso ]] || die "ISO image not found: $iso"
    iso=$(realpath -- "$iso")

    if [[ -z $state_dir ]]; then
        state_dir=${iso%/*}
    fi

    if [[ -e $state_dir && ! -d $state_dir ]]; then
        die "state path is not a directory: $state_dir"
    fi

    mkdir -p -- "$state_dir"
    state_dir=$(realpath -- "$state_dir")

    if [[ -z $debug_capture ]]; then
        debug_capture=$state_dir/net0.pcap
    elif [[ $capture != /* ]]; then
        debug_capture=$state_dir/$debug_capture
    fi
}

# Validate the debugger network configuration
check_debug() {
    if [[ -n $debug_port ]]; then
        if [[ ! $debug_port =~ ^[0-9]+$ ]] ||
           ((${#debug_port} > 5)) || ((10#$debug_port < 1 || 10#$debug_port > 65535)); then
            usage_error "debug port must be between 1 and 65535"
        fi
    fi

    [[ $debug_device != *,* ]] || usage_error "debug device must not contain commas"
}

# Copy the OVMF code and variable templates when persistent state is absent
prepare_ovmf() {
    local code=$state_dir/code.bin
    local vars=$state_dir/vars.bin

    if [[ -f $code && -f $vars ]]; then
        return
    fi

    [[ -n $ovmf_code_source ]] || die "--ovmf-code is required to initialize OVMF state"
    [[ -n $ovmf_vars_source ]] || die "--ovmf-vars is required to initialize OVMF state"
    [[ -f $ovmf_code_source ]] || die "OVMF code image not found: $ovmf_code_source"
    [[ -f $ovmf_vars_source ]] || die "OVMF variables image not found: $ovmf_vars_source"

    cp -- "$ovmf_code_source" "$code"
    cp -- "$ovmf_vars_source" "$vars"
}

# Assemble the QEMU command and replace the script process with it
run_qemu() {
    local code=$state_dir/code.bin
    local vars=$state_dir/vars.bin
    local network
    local -a args=()

    if [[ $kvm_enabled == true ]]; then
        args+=(-enable-kvm)
    fi

    args+=(
        -machine q35
        -cpu host,+invtsc
        -smp "$(nproc)"
        -drive "if=pflash,format=raw,unit=0,file=$code,readonly=on"
        -drive "if=pflash,format=raw,unit=1,file=$vars"
        -cdrom "$iso"
        -no-reboot
        -no-shutdown
    )

    if [[ $debug_enabled == true ]]; then
        if [[ -n $debug_port ]]; then
            network="user,id=net0,hostfwd=udp::$debug_port-:$debug_port"
        else
            network=user,id=net0,hostfwd=tcp::50005-:50005
        fi

        args+=(
            -netdev "$network"
            -device "$debug_device,netdev=net0"
            -object "filter-dump,id=f0,netdev=net0,file=$debug_capture"
        )
    fi

    args+=("${qemu_extra_args[@]}")
    exec "$qemu_binary" "${args[@]}"
}

main() {
    parse_args "$@"
    [[ -n $iso ]] || usage_error "--iso is required"

    command -v realpath >/dev/null || die "required tool not found: realpath"
    command -v nproc >/dev/null || die "required tool not found: nproc"
    command -v "$qemu_binary" >/dev/null || die "QEMU binary not found: $qemu_binary"

    check_paths
    check_debug
    prepare_ovmf
    run_qemu
}

main "$@"
