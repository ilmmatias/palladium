# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.21)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/sdk/cmake")
include(sdk/cmake/config.cmake)
include(sdk/cmake/toolchain.cmake)
include(sdk/cmake/project.cmake)

if(ARCH STREQUAL "x86" OR ARCH STREQUAL "amd64")
    include(sdk/cmake/bootsect.cmake)
endif()

project(os)
enable_language(ASM C CXX RC)

if (NOT ARCH STREQUAL "x86")
    add_subdirectory(drivers)
    add_subdirectory(kernel)
endif()

if (NOT ARCH STREQUAL "amd64")
    add_subdirectory(boot)
endif()

add_subdirectory(sdk)
