# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

project(os/drivers/acpi)

if(ARCH STREQUAL "amd64")
    set(SOURCES
        ${ARCH}/mmio.c
        ${ARCH}/pci.c
        ${ARCH}/port.c)
endif()

add_executable(acpi "knostdlib" YES
    ${SOURCES}

    interp/aml.c
    interp/args.c
    interp/field.c
    interp/object.c
    interp/override.c
    interp/termarg.c
    interp/termlist.c

    opcode/concat.c
    opcode/conv.c
    opcode/dataobj.c
    opcode/field.c
    opcode/lock.c
    opcode/math.c
    opcode/namedobj.c
    opcode/nsmod.c
    opcode/opcode.c
    opcode/ref.c
    opcode/stmt.c
    opcode/target.c

    acpi.def
    main.c
    os.c
    table.c)

target_link_libraries(acpi kernel)
target_include_directories(acpi PRIVATE include/private PUBLIC include/public)
target_compile_options(acpi PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
target_link_options(acpi PRIVATE -Wl,--subsystem=native,--entry=DriverEntry)
set_target_properties(acpi PROPERTIES SUFFIX ".sys")
set_target_properties(acpi PROPERTIES ENABLE_EXPORTS TRUE)
