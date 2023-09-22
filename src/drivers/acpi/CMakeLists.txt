# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

project(os/drivers/acpi)

if(ARCH STREQUAL "amd64")
    set(SOURCES
        ${ARCH}/pci.c
        ${ARCH}/port.c)
endif()

add_executable(acpi "knostdlib"
    ${SOURCES}

    interp/aml.c
    interp/field.c
    interp/object.c
    interp/termarg.c
    interp/termlist.c

    opcode/concat.c
    opcode/conv.c
    opcode/dataobj.c
    opcode/field.c
    opcode/math.c
    opcode/namedobj.c
    opcode/nsmod.c
    opcode/opcode.c
    opcode/ref.c
    opcode/stmt.c
    opcode/target.c

    main.c
    rsdt.c)

target_link_libraries(acpi kernel)
target_include_directories(acpi PRIVATE include)
target_compile_options(acpi PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
target_link_options(acpi PRIVATE -Wl,--subsystem=native,--entry=DriverEntry)
set_target_properties(acpi PROPERTIES SUFFIX ".sys")
