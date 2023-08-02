# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

project(os/boot)

if(ARCH STREQUAL "x86")
    add_subdirectory(bootsect)
    add_subdirectory(startup)
endif()

if(NOT ARCH STREQUAL "amd64")
    add_subdirectory(bootmgr)
endif()
