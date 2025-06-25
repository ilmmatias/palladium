# SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

project(os/sdk/rt)

if(ARCH STREQUAL "amd64")
    set(SOURCES
        ${ARCH}/context.S)
endif()

set(SOURCES
    ${SOURCES}
    avltree.c
    bitmap.c
    hash.c
    list.c)

include(boot.cmake)
include(kernel.cmake)
include(user.cmake)
