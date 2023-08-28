# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

project(os/kernel)

if(ARCH STREQUAL "amd64")
    set(SOURCES
        ke/${ARCH}/driver.c
        mm/${ARCH}/page.c)
endif()

add_executable(kernel "kernel"
    ${SOURCES}

    ke/entry.c

    mm/allocator.c
    mm/page.c

    kernel.def)

target_include_directories(kernel PRIVATE include)
target_compile_options(kernel PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
target_link_options(kernel PRIVATE -Wl,--subsystem=native,--entry=KeSystemStartup)
set_target_properties(kernel PROPERTIES ENABLE_EXPORTS TRUE)
