# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

project(os/boot)

if(ARCH STREQUAL "x86")
    add_subdirectory(bootsect)
endif()

add_subdirectory(bootmgr)
