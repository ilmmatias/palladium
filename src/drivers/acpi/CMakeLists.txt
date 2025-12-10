# SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

project(os/drivers/acpi)

add_library(acpi "knostdlib" SHARED
    ${SOURCES}

    interp/aml.c
    interp/args.c
    interp/field.c
    interp/lock.c
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

    os/debug.c
    os/memory.c
    os/mmio.c
    os/pci.c
    os/port.c
    os/sync.c
    os/table.c
    os/timer.c

    acpi.def
    main.c
    table.c)

target_link_libraries(acpi kernel)
target_include_directories(acpi PRIVATE include/private PUBLIC include/public)
target_link_options(acpi PRIVATE -Wl,--subsystem=native,--entry=DriverEntry)
set_target_properties(acpi PROPERTIES SUFFIX ".sys")
set_target_properties(acpi PROPERTIES ENABLE_EXPORTS TRUE)
