# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

project(os/kernel)

if(ARCH STREQUAL "amd64")
    set(SOURCES
        ke/${ARCH}/acpi.c
        ke/${ARCH}/driver.c

        mm/${ARCH}/page.c
        
        vid/${ARCH}/initialize.c)
endif()

add_executable(kernel "kernel" YES
    ${SOURCES}

    io/device.c

    ke/entry.c
    ke/panic.c

    mm/page.c
    mm/pool.c

    vid/attributes.c
    vid/font.c
    vid/print.c

    kernel.def)

target_include_directories(kernel PRIVATE include/private PUBLIC include/public)
target_compile_options(kernel PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
target_link_options(kernel PRIVATE -Wl,--subsystem=native,--entry=KiSystemStartup)
set_target_properties(kernel PROPERTIES ENABLE_EXPORTS TRUE)
