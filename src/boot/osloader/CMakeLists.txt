# SPDX-FileCopyrightText: (C) 2024-2025 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

project(os/boot/osloader)

if(ARCH STREQUAL "amd64")
    set(SOURCES
        ${ARCH}/bsp.c
        ${ARCH}/prepare.c
        ${ARCH}/transfer.c
    )
endif()

add_executable(osloader "kstdlib" NO
    ${SOURCES}
    acpi.c
    console.c
    entropy.c
    file.c
    graphics.c
    guid.c
    loader.c
    main.c
    page.c
    virt.c
)

target_include_directories(osloader PRIVATE include)
target_compile_options(osloader PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
target_compile_options(osloader PRIVATE -fshort-wchar)
target_link_options(osloader
    PRIVATE -Wl,/driver
    PRIVATE -Wl,--subsystem=efi_application
    PRIVATE -Wl,--entry=OslMain)
