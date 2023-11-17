# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

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
        hal/${ARCH}/map.c
        hal/${ARCH}/platform.c
        hal/${ARCH}/smp.c
        hal/${ARCH}/smp.S
        hal/${ARCH}/timer.c)
endif()

add_executable(kernel "kernel" YES
    ${SOURCES}

    ev/dispatch.c
    ev/dpc.c
    ev/types.c

    hal/timer.c

    io/device.c

    ke/acpi.c
    ke/driver.c
    ke/entry.c
    ke/lock.c
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

target_include_directories(kernel PRIVATE include/private PUBLIC include/public)
target_compile_options(kernel PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
target_link_options(kernel PRIVATE -Wl,--subsystem=native,--entry=KiSystemStartup)
set_target_properties(kernel PROPERTIES ENABLE_EXPORTS TRUE)
