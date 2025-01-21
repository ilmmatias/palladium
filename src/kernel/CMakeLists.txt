# SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

project(os/kernel)

if(ARCH STREQUAL "amd64")
    set(SOURCES
        hal/${ARCH}/apic.c
        hal/${ARCH}/context.c
        hal/${ARCH}/gdt.c
        hal/${ARCH}/gdt.S
        hal/${ARCH}/hpet.c
        hal/${ARCH}/idt.c
        hal/${ARCH}/idt.S
        hal/${ARCH}/ioapic.c
        hal/${ARCH}/irql.c
        hal/${ARCH}/map.c
        hal/${ARCH}/platform.c
        hal/${ARCH}/smp.c
        hal/${ARCH}/smp.S
        hal/${ARCH}/timer.c)
    set(ARCH_STR "amd64")
endif()

add_executable(kernel "kernel" YES
    ${SOURCES}

    ev/clock.c
    ev/dispatch.cxx
    ev/dpc.cxx
    ev/types.c

    hal/timer.c

    io/device.cxx

    ke/acpi.c
    ke/driver.c
    ke/entry.cxx
    ke/irql.c
    ke/lock.c
    ke/panic.c

    mm/initialize.c
    mm/page.cxx
    mm/pool.cxx

    ps/idle.c
    ps/scheduler.cxx
    ps/thread.cxx

    vid/attributes.cxx
    vid/font.c
    vid/initialize.c
    vid/print.cxx

    kernel.def)

# Get the latest abbreviated commit hash of the working branch
execute_process(
    COMMAND git log -1 --format=%h
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )

target_include_directories(kernel PRIVATE include/private PUBLIC include/public)
target_compile_options(kernel PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
target_compile_definitions(kernel PRIVATE "-DKE_GIT_HASH=\"${GIT_HASH}\"" PRIVATE "-DKE_ARCH=\"${ARCH_STR}\"")
target_link_options(kernel PRIVATE -Wl,--subsystem=native,--entry=KiBspEntry)
set_target_properties(kernel PROPERTIES ENABLE_EXPORTS TRUE)
