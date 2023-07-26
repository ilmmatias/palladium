# SPDX-FileCopyrightText: (C) 2023 yuuma03
# SPDX-License-Identifier: BSD-3-Clause

project(os/boot/bootmgr)

if(ARCH STREQUAL "x86")
    set(SOURCES
        ${ARCH}/display.c
        ${ARCH}/init.c
        ${ARCH}/memory.c)
endif()

add_executable(bootmgr "kstdlib"
    ${SOURCES}
    display.c
    main.c)

target_include_directories(bootmgr PRIVATE include)
target_compile_options(bootmgr PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
target_link_libraries(bootmgr bacpi)
target_link_options(bootmgr PRIVATE -Wl,--subsystem=native,--entry=BmMain,--image-base=0x8000,--no-dynamicbase)
