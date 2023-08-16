# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

project(os/boot/bootmgr)

if(ARCH STREQUAL "x86")
    set(SOURCES
        ${ARCH}/bios.asm
        ${ARCH}/disk.c
        ${ARCH}/display.c
        ${ARCH}/init.c
        ${ARCH}/keyboard.c
        ${ARCH}/memory.c)
endif()

add_executable(bootmgr "kstdlib"
    ${SOURCES}

    dev/console.c
    dev/display.c
    dev/memory.c

    fs/exfat.c
    fs/fat32.c
    fs/iso9660.c
    fs/ntfs.c
    fs/stdio.c
    fs/vfs.c

    assert.c
    hash.c
    main.c
    menu.c
    registry.c)

target_include_directories(bootmgr PRIVATE include)
target_compile_options(bootmgr PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
target_link_options(bootmgr PRIVATE -Wl,--subsystem=native,--entry=BmMain,--image-base=0x8000,--no-dynamicbase)
