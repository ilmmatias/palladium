# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

add_library(bexfatfs "knostdlib" STATIC ${SOURCES})

target_include_directories(bexfatfs PUBLIC include)
target_compile_options(bexfatfs PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
