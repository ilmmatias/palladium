# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

project(os/kernel)

if(ARCH STREQUAL "amd64")
    set(SOURCES
        hal/${ARCH}/acpi.c
        hal/${ARCH}/apic.c
        hal/${ARCH}/hpet.c
        hal/${ARCH}/idt.c
        hal/${ARCH}/idt.S
        hal/${ARCH}/ioapic.c
        hal/${ARCH}/platform.c

        ke/${ARCH}/driver.c

        mm/${ARCH}/map.c
        mm/${ARCH}/page.c
        
        vid/${ARCH}/initialize.c)
endif()

add_executable(kernel "kernel" YES
    ${SOURCES}

    hal/acpi.c
    hal/timer.c

    io/device.c

    ke/entry.c
    ke/lock.c
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
