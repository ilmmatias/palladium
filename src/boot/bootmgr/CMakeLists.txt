# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

project(os/boot/bootmgr)

if(ARCH STREQUAL "x86")
    set(SOURCES
        ${ARCH}/bios.S
        ${ARCH}/chainload.c
        ${ARCH}/chainload.S
        ${ARCH}/disk.c
        ${ARCH}/display.c
        ${ARCH}/keyboard.c
        ${ARCH}/memory.c
        ${ARCH}/palladium.c
        ${ARCH}/palladium.S
        ${ARCH}/platform.c
        ${ARCH}/timer.c)
endif()

add_executable(bootmgr "kstdlib" NO
    ${SOURCES}

    fs/exfat.c
    fs/fat32.c
    fs/iso9660.c
    fs/file.c
    fs/mbr.c
    fs/ntfs.c
    fs/part.c

    ldr/config.c
    ldr/ini.c
    ldr/palladium.c
    ldr/self.c

    mm/allocator.c
    mm/limits.c
    mm/md.c
    mm/page.c
    mm/virt.c

    vid/attributes.c
    vid/font.c
    vid/menu.c
    vid/print.c

    main.c)

target_include_directories(bootmgr PRIVATE include)
target_compile_options(bootmgr PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)

if(ARCH STREQUAL "x86")
    target_link_options(bootmgr
        PRIVATE -Wl,/driver
        PRIVATE -Wl,--subsystem=native
        PRIVATE -Wl,--entry=BiMain
        PRIVATE -Wl,--image-base=0x9A00
        PRIVATE -Wl,--no-dynamicbase
        PRIVATE -Wl,--file-alignment=0x200
        PRIVATE -Wl,--section-alignment=0x200)
else()
    target_link_options(bootmgr
        PRIVATE -Wl,/driver
        PRIVATE -Wl,--subsystem=native
        PRIVATE -Wl,--entry=BiMain
        PRIVATE -Wl,--file-alignment=0x200
        PRIVATE -Wl,--section-alignment=0x200)
endif()
