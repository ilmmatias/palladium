#!/bin/bash

cmake                                                                     \
    -S $COMPILER_RT_PATH                                                  \
    -B build.compiler-rt                                                  \
    -G Ninja                                                              \
    -DCMAKE_AR=$(which llvm-ar$LLVM_SUFFIX)                               \
    -DCMAKE_ASM_COMPILER_TARGET=$COMPILER_RT_TARGET-w64-mingw32           \
    -DCMAKE_ASM_FLAGS="-I$PALLADIUM_PATH/src/sdk/crt/include"             \
    -DCMAKE_C_COMPILER=$(which clang$LLVM_SUFFIX)                         \
    -DCMAKE_C_COMPILER_WORKS=true                                         \
    -DCMAKE_C_COMPILER_TARGET=$COMPILER_RT_TARGET-w64-mingw32             \
    -DCMAKE_C_FLAGS="-I$PALLADIUM_PATH/src/sdk/crt/include -mno-red-zone" \
    -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld"                               \
    -DCMAKE_NM=$(which llvm-nm$LLVM_SUFFIX)                               \
    -DCMAKE_RANLIB=$(which llvm-ranlib$LLVM_SUFFIX)                       \
    -DCOMPILER_RT_OS_DIR=baremetal                                        \
    -DCOMPILER_RT_BAREMETAL_BUILD=ON                                      \
    -DCOMPILER_RT_BUILD_BUILTINS=ON                                       \
    -DCOMPILER_RT_BUILD_CRT=OFF                                           \
    -DCOMPILER_RT_BUILD_CTX_PROFILE=OFF                                   \
    -DCOMPILER_RT_BUILD_LIBFUZZER=OFF                                     \
    -DCOMPILER_RT_BUILD_MEMPROF=OFF                                       \
    -DCOMPILER_RT_BUILD_ORC=OFF                                           \
    -DCOMPILER_RT_BUILD_PROFILE=OFF                                       \
    -DCOMPILER_RT_BUILD_SANITIZERS=OFF                                    \
    -DCOMPILER_RT_BUILD_XRAY=OFF                                          \
    -DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON                                  \
    -DLLVM_CMAKE_DIR=$(llvm-config$LLVM_SUFFIX --cmakedir)

cmake --build build.compiler-rt
sudo cmake --build build.compiler-rt --target install
