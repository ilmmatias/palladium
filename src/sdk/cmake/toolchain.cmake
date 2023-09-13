# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

set(CMAKE_SYSTEM_NAME Palladium)
set(CMAKE_SYSTEM_PROCESSOR ${ARCH})

set(CMAKE_C_COMPILER clang)
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER clang)
set(CMAKE_CXX_COMPILER_WORKS 1)
set(CMAKE_ASM_COMPILER clang)
set(CMAKE_AR llvm-ar)

# We're missing a message compiler for now (probably just build the one from binutils?)
set(CMAKE_RC_COMPILER llvm-windres)
set(CMAKE_RC_FLAGS --target=${TARGET_${ARCH}}-w64-mingw32)
