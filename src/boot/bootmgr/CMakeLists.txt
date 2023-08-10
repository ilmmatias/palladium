# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

project(os/boot/bootmgr)

if(ARCH STREQUAL "x86")
    set(SOURCES
        ${ARCH}/bios.asm
        ${ARCH}/disk.c
        ${ARCH}/display.c
        ${ARCH}/init.c
        ${ARCH}/memory.c)
endif()

add_executable(bootmgr "kstdlib"
    ${SOURCES}

    fs/exfat.c
    fs/iso9660.c
    fs/ntfs.c
    fs/vfs.c

    support/assert.c
    support/display.c
    support/memory.c

    main.c)

target_link_libraries(bootmgr
    bexfatfs
    bfat32fs
    biso9660fs
    bntfs
)

target_include_directories(bootmgr PRIVATE include)
target_compile_options(bootmgr PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
target_link_options(bootmgr PRIVATE -Wl,--subsystem=native,--entry=BmMain,--image-base=0x8000,--no-dynamicbase)
