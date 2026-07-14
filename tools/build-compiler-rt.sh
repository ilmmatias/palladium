#!/usr/bin/env bash
# SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

# Required environment: COMPILER_RT_PATH, COMPILER_RT_TARGET, PALLADIUM_PATH
# Optional environment: LLVM_SUFFIX

: "${COMPILER_RT_PATH:?COMPILER_RT_PATH is not set}"
: "${COMPILER_RT_TARGET:?COMPILER_RT_TARGET is not set}"
: "${PALLADIUM_PATH:?PALLADIUM_PATH is not set}"

LLVM_SUFFIX=${LLVM_SUFFIX:-}

command_path() {
    command -v "$1" || {
        printf 'Error: required tool not found: %s\n' "$1" >&2
        exit 1
    }
}

build_dir=build.compiler-rt
target=$COMPILER_RT_TARGET-w64-mingw32
include_dir=$PALLADIUM_PATH/src/sdk/crt/include

clang=$(command_path "clang$LLVM_SUFFIX")
llvm_ar=$(command_path "llvm-ar$LLVM_SUFFIX")
llvm_config=$(command_path "llvm-config$LLVM_SUFFIX")
llvm_nm=$(command_path "llvm-nm$LLVM_SUFFIX")
llvm_ranlib=$(command_path "llvm-ranlib$LLVM_SUFFIX")
llvm_cmake_dir=$("$llvm_config" --cmakedir)

command_path cmake >/dev/null
command_path ninja >/dev/null
command_path sudo >/dev/null

cmake \
    -S "$COMPILER_RT_PATH" \
    -B "$build_dir" \
    -G Ninja \
    "-DCMAKE_AR=$llvm_ar" \
    "-DCMAKE_ASM_COMPILER_TARGET=$target" \
    "-DCMAKE_ASM_FLAGS=-I$include_dir" \
    "-DCMAKE_C_COMPILER=$clang" \
    -DCMAKE_C_COMPILER_WORKS=true \
    "-DCMAKE_C_COMPILER_TARGET=$target" \
    "-DCMAKE_C_FLAGS=-I$include_dir -mno-red-zone" \
    -DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld \
    "-DCMAKE_NM=$llvm_nm" \
    "-DCMAKE_RANLIB=$llvm_ranlib" \
    -DCOMPILER_RT_OS_DIR=baremetal \
    -DCOMPILER_RT_BAREMETAL_BUILD=ON \
    -DCOMPILER_RT_BUILD_BUILTINS=ON \
    -DCOMPILER_RT_BUILD_CRT=OFF \
    -DCOMPILER_RT_BUILD_CTX_PROFILE=OFF \
    -DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
    -DCOMPILER_RT_BUILD_MEMPROF=OFF \
    -DCOMPILER_RT_BUILD_ORC=OFF \
    -DCOMPILER_RT_BUILD_PROFILE=OFF \
    -DCOMPILER_RT_BUILD_SANITIZERS=OFF \
    -DCOMPILER_RT_BUILD_XRAY=OFF \
    -DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON \
    "-DLLVM_CMAKE_DIR=$llvm_cmake_dir"

cmake --build "$build_dir"
sudo cmake --build "$build_dir" --target install
