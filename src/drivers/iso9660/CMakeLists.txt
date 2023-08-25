# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

project(os/drivers/iso9660)

add_executable(iso9660 "kstdlib"
    main.c)

target_include_directories(iso9660 PRIVATE include)
target_compile_options(iso9660 PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
target_link_options(iso9660 PRIVATE -Wl,--subsystem=native,--entry=DrvMain)
set_target_properties(iso9660 PROPERTIES SUFFIX ".sys")
