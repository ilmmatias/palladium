# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

project(os/boot/bootmgr)

if(ARCH STREQUAL "x86")
    set(SOURCES
        ${ARCH}/arch.c
        ${ARCH}/bios.S
        ${ARCH}/disk.c
        ${ARCH}/display.c
        ${ARCH}/keyboard.c
        ${ARCH}/memory.c
        ${ARCH}/transfer.c
        ${ARCH}/transfer.S)
endif()

add_executable(bootmgr "kstdlib" NO
    ${SOURCES}

    fs/console.c
    fs/exfat.c
    fs/fat32.c
    fs/iso9660.c
    fs/ntfs.c
    fs/stdio.c
    fs/vfs.c

    loader/menu.c
    loader/palladium.c

    vid/font.c
    vid/print.c

    allocator.c
    assert.c
    main.c
    memory.c
    registry.c)

target_include_directories(bootmgr PRIVATE include)
target_compile_options(bootmgr PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
target_link_options(bootmgr PRIVATE -Wl,--subsystem=native,--entry=BmMain,--image-base=0x8000,--no-dynamicbase)
