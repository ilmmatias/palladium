# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

add_library(krt "kcrt" STATIC ${SOURCES})

target_include_directories(krt PUBLIC include)
target_compile_options(krt PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
