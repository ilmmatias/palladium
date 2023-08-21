# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

project(os/kernel)

add_executable(kernel "kstdlib"
    ${SOURCES}

    main.c)

target_include_directories(kernel PRIVATE include)
target_compile_options(kernel PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
target_link_options(kernel PRIVATE -Wl,--subsystem=native,--entry=KernelMain)
