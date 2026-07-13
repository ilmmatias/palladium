#!/usr/bin/env bash
# SPDX-FileCopyrightText: (C) 2026 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

if (($# != 1)); then
    printf 'usage: %s BUILD_DIR\n' "$0" >&2
    exit 2
fi

build_dir=$1
if [[ ! -f $build_dir/compile_commands.json ]]; then
    printf 'compile database not found: %s/compile_commands.json\n' "$build_dir" >&2
    exit 1
fi

checks='-*,clang-analyzer-core.NullDereference,clang-analyzer-core.UndefinedBinaryOperatorResult,clang-analyzer-core.DivideZero,clang-analyzer-core.BitwiseShift,clang-analyzer-core.uninitialized.UndefReturn,clang-analyzer-core.CallAndMessage,clang-analyzer-security.ArrayBound'
runner=${RUN_CLANG_TIDY:-run-clang-tidy${LLVM_SUFFIX:-}}
if ! command -v "$runner" >/dev/null 2>&1; then
    printf 'clang-tidy runner not found: %s\n' "$runner" >&2
    exit 1
fi

"$runner" \
    -p "$build_dir" \
    -checks="$checks" \
    -warnings-as-errors='*' \
    -source-filter='.*\.(c)$'
