# SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

project(os/sdk/rt)

if(ARCH STREQUAL "amd64")
    set(SOURCES
        ${ARCH}/context.S)
endif()

set(SOURCES
    ${SOURCES}
    bitmap.c
    hash.c
    list.c
    placeholder.c)

include(kernel.cmake)
include(user.cmake)
