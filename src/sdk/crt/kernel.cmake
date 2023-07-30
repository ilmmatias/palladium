# SPDX-FileCopyrightText: (C) 2023 yuuma03
# SPDX-License-Identifier: BSD-3-Clause

add_library(kcrt "knostdlib" STATIC ${SOURCES})

target_include_directories(kcrt PUBLIC include)
target_compile_options(kcrt PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
