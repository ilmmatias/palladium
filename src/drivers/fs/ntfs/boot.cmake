# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

add_library(bntfs "kstdlib" STATIC ${SOURCES})

target_include_directories(bntfs PUBLIC include/public)
target_include_directories(bntfs PRIVATE include/private)
target_compile_options(bntfs PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
