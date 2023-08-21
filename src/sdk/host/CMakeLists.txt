# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.21)

project(host)
enable_language(C CXX)

add_executable(create-boot-registry create-boot-registry.c ../rt/hash.c)
target_include_directories(create-boot-registry PRIVATE include ../rt/include)
