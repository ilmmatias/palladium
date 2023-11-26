# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

if(ARCH STREQUAL "amd64")
    set(KERNEL_SOURCES
        ${ARCH}/except.c
        ${ARCH}/unwind.c)
endif()

set(KERNEL_SOURCES
    ${KERNEL_SOURCES}
    image.c)

add_library(krt "kcrt" STATIC ${SOURCES} ${KERNEL_SOURCES})

target_include_directories(krt
    PUBLIC include
    PRIVATE ../../kernel/include/public)

target_compile_options(krt PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
