# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

project(os/drivers/fat32)

add_executable(fat32 "knostdlib"
    main.c)

target_link_libraries(fat32 kernel)
target_include_directories(fat32 PRIVATE include)
target_compile_options(fat32 PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
target_link_options(fat32 PRIVATE -Wl,--subsystem=native,--entry=DriverEntry)
set_target_properties(fat32 PROPERTIES SUFFIX ".sys")
