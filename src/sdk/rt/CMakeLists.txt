# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

project(os/sdk/rt)

set(SOURCES
    ${SOURCES}
    double_list.c
    hash.c
    placeholder.c
    single_list.c)

include(kernel.cmake)

if(NOT ARCH STREQUAL "x86")
    include(user.cmake)
endif()
