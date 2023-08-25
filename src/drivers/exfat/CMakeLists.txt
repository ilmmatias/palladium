# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

project(os/drivers/exfat)

add_executable(exfat "kstdlib"
    main.c)

target_include_directories(exfat PRIVATE include)
target_compile_options(exfat PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
target_link_options(exfat PRIVATE -Wl,--subsystem=native,--entry=DrvMain)
set_target_properties(exfat PROPERTIES SUFFIX ".sys")
