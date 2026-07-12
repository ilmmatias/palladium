#!/usr/bin/env bash
# SPDX-FileCopyrightText: (C) 2026 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

usage() {
    cat <<'EOF'
Usage: tools/smoke.sh --build-dir DIR --output-dir DIR \
    --ovmf-code PATH --ovmf-vars PATH [--repeat COUNT] [--timeout SECONDS]

Build a proprietary-free diagnostic image and run the fixed amd64 QEMU TCG smoke profile.
EOF
}

build_dir=
output_dir=
ovmf_code=
ovmf_vars=
repeat=1
timeout=60

while (($#)); do
    case "$1" in
        --build-dir) build_dir=${2-}; shift 2 ;;
        --output-dir) output_dir=${2-}; shift 2 ;;
        --ovmf-code) ovmf_code=${2-}; shift 2 ;;
        --ovmf-vars) ovmf_vars=${2-}; shift 2 ;;
        --repeat) repeat=${2-}; shift 2 ;;
        --timeout) timeout=${2-}; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) printf 'unknown argument: %s\n' "$1" >&2; usage >&2; exit 2 ;;
    esac
done

if [[ -z $build_dir || -z $output_dir || -z $ovmf_code || -z $ovmf_vars ]]; then
    usage >&2
    exit 2
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

repo_dir=$(cd "$(dirname "$0")/.." && pwd)
image_dir=$output_dir/image
run_dir=$output_dir/qemu

"$repo_dir/tools/build-image.sh" \
    --build-dir "$build_dir" \
    --output-dir "$image_dir" \
    --diagnostic-serial
"$repo_dir/tools/run-qemu.py" smoke \
    --image "$image_dir/iso9660.iso" \
    --ovmf-code "$ovmf_code" \
    --ovmf-vars "$ovmf_vars" \
    --output-dir "$run_dir" \
    --repeat "$repeat" \
    --timeout "$timeout"
