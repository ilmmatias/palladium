# SPDX-FileCopyrightText: (C) 2023 yuuma03
# SPDX-License-Identifier: BSD-3-Clause

project(os/sdk/crt)

if(ARCH STREQUAL "amd64")
    set(SOURCES
        string/${ARCH}/memcpy.asm
        string/${ARCH}/memset.asm
        string/${ARCH}/strlen.asm)
else()
    set(SOURCES
        string/memcpy.c
        string/memset.c
        string/strlen.c)
endif()

set(SOURCES
    ${SOURCES}
    placeholder.c)

include(kernel.cmake)
include(user.cmake)
