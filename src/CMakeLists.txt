# SPDX-FileCopyrightText: (C) 2023 yuuma03
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.21)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/sdk/cmake")
include(sdk/cmake/config.cmake)
include(sdk/cmake/toolchain.cmake)
include(sdk/cmake/project.cmake)

if(ARCH STREQUAL "x86" OR ARCH STREQUAL "amd64")
    include(sdk/cmake/bootsect.cmake)
endif()

project(os)

if(ARCH STREQUAL "x86" OR ARCH STREQUAL "amd64")
    enable_language(ASM_MASM C CXX RC)
else()
    enable_language(ASM C CXX RC)
endif()

if(NOT ARCH STREQUAL "x86")
    add_subdirectory(acpi)
endif()

add_subdirectory(boot)
add_subdirectory(sdk)
