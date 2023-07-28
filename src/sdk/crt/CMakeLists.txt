# SPDX-FileCopyrightText: (C) 2023 yuuma03
# SPDX-License-Identifier: BSD-3-Clause

project(os/sdk/crt)

set(SOURCES
    placeholder.c
    string/memcpy.c
    string/memmove.c
    string/memset.c
    string/strlen.c)

include(kernel.cmake)
include(user.cmake)
