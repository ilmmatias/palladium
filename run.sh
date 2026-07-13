#!/usr/bin/env bash
# SPDX-FileCopyrightText: (C) 2026 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

# Compatibility wrapper for the historical build-directory workflow.
repo_dir=$(cd "$(dirname "$0")" && pwd)
build_dir=$PWD
artifacts=${PALLADIUM_RUN_OUTPUT:-$build_dir/qemu-run}
image_dir=$artifacts/image
qemu_dir=$artifacts/qemu

builder=("$repo_dir/tools/build-image.sh" --build-dir "$build_dir" --output-dir "$image_dir")
if [[ -n ${BOOT_DRIVERS:-} ]]; then
    builder+=(--no-default-acpi)
    for driver in $BOOT_DRIVERS; do
        case ${driver^^} in
            ACPI) builder+=(--boot-driver "ACPI=$build_dir/drivers/acpi/acpi.sys") ;;
            *) printf 'unsupported boot driver: %s\n' "$driver" >&2; exit 2 ;;
        esac
    done
fi
[[ ${BOOT_DEBUG_ENABLED:-false} != true ]] || builder+=(--debug-enabled)
[[ ${BOOT_DEBUG_ECHO_ENABLED:-false} != true ]] || builder+=(--debug-echo-enabled)
[[ -z ${BOOT_DEBUG_ADDRESS:-} ]] || builder+=(--debug-address "$BOOT_DEBUG_ADDRESS")
[[ -z ${BOOT_DEBUG_PORT:-} ]] || builder+=(--debug-port "$BOOT_DEBUG_PORT")
for module in ${BOOT_DEBUG_DRIVERS:-}; do
    builder+=(--debug-module "${BOOT_DEBUG_DRIVER_PREFIX:?BOOT_DEBUG_DRIVER_PREFIX is required}/$module")
done
"${builder[@]}"

runner=(python3 "$repo_dir/tools/run-qemu.py" interactive --image "$image_dir/iso9660.iso"
    --ovmf-code "${OVMF_CODE_PATH:?OVMF_CODE_PATH is required}"
    --ovmf-vars "${OVMF_VARS_PATH:?OVMF_VARS_PATH is required}" --output-dir "$qemu_dir")
extra_qemu=()
if [[ ${BOOT_DEBUG_ENABLED:-false} == true ]]; then
    port=${BOOT_DEBUG_PORT:-50005}
    extra_qemu+=(-netdev "user,id=net0,hostfwd=udp::$port-:$port"
        -device "${BOOT_DEBUG_DEVICE:-e1000},netdev=net0"
        -object "filter-dump,id=f0,netdev=net0,file=$artifacts/net0.pcap")
fi
extra_qemu+=("$@")
if ((${#extra_qemu[@]})); then
    runner+=(-- "${extra_qemu[@]}")
fi
exec "${runner[@]}"
