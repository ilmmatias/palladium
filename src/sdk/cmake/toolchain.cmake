# SPDX-FileCopyrightText: (C) 2023 yuuma03
# SPDX-License-Identifier: BSD-3-Clause

set(CMAKE_SYSTEM_NAME Palladium)
set(CMAKE_SYSTEM_PROCESSOR ${ARCH})

set(CMAKE_C_COMPILER clang)
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER clang)
set(CMAKE_CXX_COMPILER_WORKS 1)
set(CMAKE_AR llvm-ar)

# We use a MASM-compatible assembler on x86 and amd64 (llvm-ml/ml64), and just the default clang/gas-based assembler otherwise.
if(ARCH STREQUAL "amd64")
    set(CMAKE_ASM_MASM_COMPILER llvm-ml64)
elseif(ARCH STREQUAL "x86")
set(CMAKE_ASM_MASM_COMPILER llvm-ml)
else()
    set(CMAKE_ASM_COMPILER clang)
endif()

set(CMAKE_AR llvm-ar)

# We're missing a message compiler for now (probably just build the one from binutils?)
set(CMAKE_RC_COMPILER llvm-windres)
set(CMAKE_RC_FLAGS --target=${TARGET_${ARCH}}-w64-mingw32)
