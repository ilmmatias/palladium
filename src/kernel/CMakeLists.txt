# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

project(os/kernel)

if(ARCH STREQUAL "amd64")
    set(SOURCES
        ke/${ARCH}/driver.c

        mm/${ARCH}/page.c

        vid/${ARCH}/attributes.c
        vid/${ARCH}/initialize.c
        vid/${ARCH}/print.c)
endif()

add_executable(kernel "kernel"
    ${SOURCES}

    ke/entry.c
    ke/panic.c

    mm/allocator.c
    mm/page.c

    vid/print.c

    kernel.def)

target_include_directories(kernel PUBLIC include)
target_compile_options(kernel PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
target_link_options(kernel PRIVATE -Wl,--subsystem=native,--entry=KiSystemStartup)
set_target_properties(kernel PROPERTIES ENABLE_EXPORTS TRUE)
