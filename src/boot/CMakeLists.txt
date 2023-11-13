# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

project(os/boot)

if(ARCH STREQUAL "x86")
    add_subdirectory(bootsect)
endif()

add_subdirectory(bootmgr)
