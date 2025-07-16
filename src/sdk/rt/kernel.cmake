# SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

if(ARCH STREQUAL "amd64")
    set(KERNEL_SOURCES
        ${ARCH}/except.c
        ${ARCH}/stacktrace.c
        ${ARCH}/unwind.c)
endif()

set(KERNEL_SOURCES
    ${KERNEL_SOURCES}
    image.c)

add_library(krt "kcrt" STATIC ${SOURCES} ${KERNEL_SOURCES})

target_include_directories(krt
    PUBLIC include
    PRIVATE ../../kernel/include/public)

target_compile_options(krt PRIVATE $<$<COMPILE_LANGUAGE:C>:-ffreestanding>)
