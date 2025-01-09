# SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.21)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/sdk/cmake")
include(sdk/cmake/config.cmake)
include(sdk/cmake/toolchain.cmake)
include(sdk/cmake/project.cmake)

project(os)
enable_language(ASM C CXX RC)

add_subdirectory(boot)
add_subdirectory(drivers)
add_subdirectory(kernel)
add_subdirectory(sdk)
