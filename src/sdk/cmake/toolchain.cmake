# SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

set(CMAKE_SYSTEM_NAME Palladium)
set(CMAKE_SYSTEM_PROCESSOR ${ARCH})

set(CMAKE_C_COMPILER clang${LLVM_SUFFIX})
set(CMAKE_ASM_COMPILER clang${LLVM_SUFFIX})
set(CMAKE_RC_COMPILER llvm-windres${LLVM_SUFFIX})
set(CMAKE_AR llvm-ar${LLVM_SUFFIX})

set(CMAKE_C_COMPILER_WORKS 1)

# Disable --dependency-file, it's not supported for .exe (PE) targets
set(CMAKE_C_LINKER_DEPFILE_SUPPORTED FALSE)
