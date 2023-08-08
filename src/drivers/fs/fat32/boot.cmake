# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

add_library(bfat32fs "kstdlib" STATIC ${SOURCES})

target_include_directories(bfat32fs PUBLIC include/public)
target_include_directories(bfat32fs PRIVATE include/private)
target_compile_options(bfat32fs PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
