# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

add_library(bntfs "knostdlib" STATIC ${SOURCES})

target_include_directories(bntfs PUBLIC include)
target_compile_options(bntfs PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
