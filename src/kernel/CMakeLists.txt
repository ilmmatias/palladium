# SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

project(os/kernel)

if(ARCH STREQUAL "amd64")
    set(SOURCES
        hal/${ARCH}/apic.c
        hal/${ARCH}/context.c
        hal/${ARCH}/context.S
        hal/${ARCH}/gdt.c
        hal/${ARCH}/gdt.S
        hal/${ARCH}/hpet.c
        hal/${ARCH}/idt.c
        hal/${ARCH}/idt.S
        hal/${ARCH}/ioapic.c
        hal/${ARCH}/map.c
        hal/${ARCH}/platform.c
        hal/${ARCH}/smp.c
        hal/${ARCH}/smp.S
        hal/${ARCH}/timer.c)
    set(ARCH_STR "amd64")
endif()

add_executable(kernel "kernel" YES
    ${SOURCES}

    ev/timer.c

    hal/interrupt.c
    hal/timer.c

    ke/acpi.c
    ke/affinity.c
    ke/driver.c
    ke/entry.c
    ke/panic.c

    mm/initialize.c
    mm/page.c
    mm/pool.c

    ps/idle.c
    ps/scheduler.c
    ps/thread.c

    vid/attributes.c
    vid/font.c
    vid/initialize.c
    vid/print.c

    kernel.def)

# Get the latest abbreviated commit hash of the working branch
execute_process(
    COMMAND git log -1 --format=%h
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )

target_include_directories(kernel PRIVATE include/private PUBLIC include/public)
target_compile_options(kernel PRIVATE $<$<COMPILE_LANGUAGE:C>:-ffreestanding>)
target_compile_definitions(kernel PRIVATE "-DKE_GIT_HASH=\"${GIT_HASH}\"" PRIVATE "-DKE_ARCH=\"${ARCH_STR}\"")
target_link_options(kernel PRIVATE -Wl,--subsystem=native,--entry=KiSystemStartup)
set_target_properties(kernel PROPERTIES ENABLE_EXPORTS TRUE)
