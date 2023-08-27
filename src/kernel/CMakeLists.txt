# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

project(os/kernel)

add_executable(kernel "kernel"
    ${SOURCES}

    kernel.def
    main.c)

target_include_directories(kernel PRIVATE include)
target_compile_options(kernel PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
target_link_options(kernel PRIVATE -Wl,--subsystem=native,--entry=KeMain)
set_target_properties(kernel PROPERTIES ENABLE_EXPORTS TRUE)
